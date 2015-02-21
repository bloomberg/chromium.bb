// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/copresence/chrome_whispernet_client.h"
#include "chrome/browser/extensions/api/audio_modem/audio_modem_api.h"
#include "chrome/common/extensions/api/audio_modem.h"
#include "extensions/browser/event_router.h"

// TODO(ckehoe): Implement transmit fail checking.

namespace extensions {

using api::audio_modem::AUDIOBAND_AUDIBLE;
using api::audio_modem::AUDIOBAND_INAUDIBLE;
using api::audio_modem::Audioband;
using api::audio_modem::STATUS_CODERERROR;
using api::audio_modem::STATUS_INUSE;
using api::audio_modem::STATUS_INVALIDREQUEST;
using api::audio_modem::STATUS_SUCCESS;
using api::audio_modem::ReceivedToken;
using api::audio_modem::RequestParams;
using api::audio_modem::Status;

namespace Transmit = api::audio_modem::Transmit;
namespace StopTransmit = api::audio_modem::StopTransmit;
namespace Receive = api::audio_modem::Receive;
namespace StopReceive = api::audio_modem::StopReceive;
namespace OnReceived = api::audio_modem::OnReceived;

using audio_modem::AUDIBLE;
using audio_modem::AudioToken;
using audio_modem::AudioType;
using audio_modem::INAUDIBLE;
using audio_modem::TokenParameters;

namespace {

const char kInitFailedError[] = "The audio modem is not available. "
    "Failed to initialize the token encoder/decoder.";
const char kInvalidTokenLengthError[] =
    "The token length must be greater than zero.";
const char kIncorrectTokenLengthError[] =
    "The token provided did not match the declared token length.";
const char kInvalidTimeoutError[] =
    "Transmit and receive timeouts must be greater than zero.";

const int kMaxTransmitTimeout = 10 * 60 * 1000;  // 10 minutes
const int kMaxReceiveTimeout = 60 * 60 * 1000;  // 1 hour

base::LazyInstance<BrowserContextKeyedAPIFactory<AudioModemAPI>>
    g_factory = LAZY_INSTANCE_INITIALIZER;

AudioType AudioTypeForBand(Audioband band) {
  switch (band) {
    case AUDIOBAND_AUDIBLE:
      return AUDIBLE;
    case AUDIOBAND_INAUDIBLE:
      return INAUDIBLE;
    default:
      NOTREACHED();
      return audio_modem::AUDIO_TYPE_UNKNOWN;
  }
}

TokenParameters TokenParamsForEncoding(
    const api::audio_modem::TokenEncoding& encoding) {
  return TokenParameters(encoding.token_length,
                         encoding.crc ? *encoding.crc : false,
                         encoding.parity ? *encoding.parity : true);
}

const std::string DecodeBase64Token(std::string encoded_token) {
  // Make sure the token is padded correctly.
  while (encoded_token.size() % 4 > 0)
    encoded_token += "=";

  // Decode and return the token.
  std::string raw_token;
  bool decode_success = base::Base64Decode(encoded_token, &raw_token);
  DCHECK(decode_success);
  return raw_token;
}

}  // namespace


// Public functions.

AudioModemAPI::AudioModemAPI(content::BrowserContext* context)
    : AudioModemAPI(context,
                    make_scoped_ptr(new ChromeWhispernetClient(context)),
                    audio_modem::Modem::Create()) {}

AudioModemAPI::AudioModemAPI(
    content::BrowserContext* context,
    scoped_ptr<audio_modem::WhispernetClient> whispernet_client,
    scoped_ptr<audio_modem::Modem> modem)
    : browser_context_(context),
      whispernet_client_(whispernet_client.Pass()),
      modem_(modem.Pass()),
      init_failed_(false) {
  // We own these objects, so these callbacks will not outlive us.
  whispernet_client_->Initialize(
      base::Bind(&AudioModemAPI::WhispernetInitComplete,
                 base::Unretained(this)));
  modem_->Initialize(whispernet_client_.get(),
                     base::Bind(&AudioModemAPI::TokensReceived,
                                base::Unretained(this)));
}

AudioModemAPI::~AudioModemAPI() {
  for (const auto& timer_entry : receive_timers_[0])
    delete timer_entry.second;
  for (const auto& timer_entry : receive_timers_[1])
    delete timer_entry.second;
}

Status AudioModemAPI::StartTransmit(const std::string& app_id,
                                    const RequestParams& params,
                                    const std::string& token) {
  AudioType audio_type = AudioTypeForBand(params.band);
  if (transmitters_[audio_type].empty())
    transmitters_[audio_type] = app_id;
  else if (transmitters_[audio_type] != app_id)
    return STATUS_INUSE;

  DVLOG(3) << "Starting transmit for app " << app_id;

  std::string encoded_token;
  base::Base64Encode(token, &encoded_token);
  base::RemoveChars(encoded_token, "=", &encoded_token);

  modem_->SetTokenParams(audio_type, TokenParamsForEncoding(params.encoding));
  modem_->SetToken(audio_type, encoded_token);
  modem_->StartPlaying(audio_type);

  transmit_timers_[audio_type].Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(params.timeout_millis),
      base::Bind(base::IgnoreResult(&AudioModemAPI::StopTransmit),
                 base::Unretained(this),
                 app_id,
                 audio_type));
  return STATUS_SUCCESS;
}

Status AudioModemAPI::StopTransmit(const std::string& app_id,
                                   AudioType audio_type) {
  if (transmitters_[audio_type] != app_id)
    return STATUS_INVALIDREQUEST;

  DVLOG(3) << "Stopping transmit for app " << app_id;
  transmitters_[audio_type].clear();
  modem_->StopPlaying(audio_type);
  return STATUS_SUCCESS;
}

void AudioModemAPI::StartReceive(const std::string& app_id,
                                 const RequestParams& params) {
  DVLOG(3) << "Starting receive for app " << app_id;

  AudioType audio_type = AudioTypeForBand(params.band);
  modem_->SetTokenParams(audio_type, TokenParamsForEncoding(params.encoding));
  modem_->StartRecording(audio_type);

  if (receive_timers_[audio_type].count(app_id) == 0)
    receive_timers_[audio_type][app_id] = new base::OneShotTimer<AudioModemAPI>;
  DCHECK(receive_timers_[audio_type][app_id]);
  receive_timers_[audio_type][app_id]->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(params.timeout_millis),
      base::Bind(base::IgnoreResult(&AudioModemAPI::StopReceive),
                 base::Unretained(this),
                 app_id,
                 audio_type));
}

Status AudioModemAPI::StopReceive(const std::string& app_id,
                                  AudioType audio_type) {
  if (receive_timers_[audio_type].count(app_id) == 0)
    return STATUS_INVALIDREQUEST;

  DCHECK(receive_timers_[audio_type][app_id]);
  delete receive_timers_[audio_type][app_id];
  receive_timers_[audio_type].erase(app_id);

  DVLOG(3) << "Stopping receive for app " << app_id;
  if (receive_timers_[audio_type].empty())
    modem_->StopRecording(audio_type);
  return STATUS_SUCCESS;
}

// static
BrowserContextKeyedAPIFactory<AudioModemAPI>*
AudioModemAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}


// Private functions.

void AudioModemAPI::WhispernetInitComplete(bool success) {
  if (success) {
    VLOG(2) << "Whispernet initialized successfully.";
  } else {
    LOG(ERROR) << "Failed to initialize Whispernet!";
    init_failed_ = true;
  }
}

void AudioModemAPI::TokensReceived(const std::vector<AudioToken>& tokens) {
  // Distribute the tokens to the appropriate app(s).
  std::map<std::string, std::vector<linked_ptr<ReceivedToken>>> tokens_by_app;
  for (const AudioToken& token : tokens) {
    linked_ptr<ReceivedToken> api_token(new ReceivedToken);
    const std::string& raw_token = DecodeBase64Token(token.token);
    api_token->token.assign(raw_token.c_str(),
                            raw_token.c_str() + raw_token.size());
    api_token->band = token.audible ? AUDIOBAND_AUDIBLE : AUDIOBAND_INAUDIBLE;
    for (const auto& receiver :
         receive_timers_[token.audible ? AUDIBLE : INAUDIBLE]) {
      tokens_by_app[receiver.first].push_back(api_token);
    }
  }

  // Send events to the appropriate app(s).
  for (const auto& app_entry : tokens_by_app) {
    const std::string& app_id = app_entry.first;
    const auto& tokens = app_entry.second;
    if (app_id.empty())
      continue;

    EventRouter::Get(browser_context_)->DispatchEventToExtension(
      app_id,
      make_scoped_ptr(new Event(OnReceived::kEventName,
                                OnReceived::Create(tokens))));
  }
}


// Functions outside the API scope.

template <>
void
BrowserContextKeyedAPIFactory<AudioModemAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

ExtensionFunction::ResponseAction AudioModemTransmitFunction::Run() {
  scoped_ptr<Transmit::Params> params(Transmit::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  AudioModemAPI* api =
      AudioModemAPI::GetFactoryInstance()->Get(browser_context());
  if (api->init_failed()) {
    return RespondNow(ErrorWithArguments(
        Transmit::Results::Create(STATUS_CODERERROR),
        kInitFailedError));
  }

  // Check the token length.
  int token_length = params->params.encoding.token_length;
  if (token_length <= 0) {
    return RespondNow(ErrorWithArguments(
        Transmit::Results::Create(STATUS_INVALIDREQUEST),
        kInvalidTokenLengthError));
  }
  const char* token = vector_as_array(&params->token);
  std::string token_str(token, params->token.size());
  if (static_cast<int>(token_str.size()) != token_length) {
    return RespondNow(ErrorWithArguments(
        Transmit::Results::Create(STATUS_INVALIDREQUEST),
        kIncorrectTokenLengthError));
  }

  // Check the timeout.
  int64_t timeout_millis = params->params.timeout_millis;
  if (timeout_millis <= 0) {
    return RespondNow(ErrorWithArguments(
        Transmit::Results::Create(STATUS_INVALIDREQUEST),
        kInvalidTimeoutError));
  }
  if (timeout_millis > kMaxTransmitTimeout)
    timeout_millis = kMaxTransmitTimeout;

  // Start transmission.
  Status status = api->StartTransmit(extension_id(), params->params, token_str);
  return RespondNow(ArgumentList(Transmit::Results::Create(status)));
}

ExtensionFunction::ResponseAction AudioModemStopTransmitFunction::Run() {
  scoped_ptr<StopTransmit::Params> params(StopTransmit::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Status status = AudioModemAPI::GetFactoryInstance()->Get(browser_context())
      ->StopTransmit(extension_id(), AudioTypeForBand(params->band));
  return RespondNow(ArgumentList(StopTransmit::Results::Create(status)));
}

ExtensionFunction::ResponseAction AudioModemReceiveFunction::Run() {
  scoped_ptr<Receive::Params> params(Receive::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  AudioModemAPI* api =
      AudioModemAPI::GetFactoryInstance()->Get(browser_context());
  if (api->init_failed()) {
    return RespondNow(ErrorWithArguments(
        Transmit::Results::Create(STATUS_CODERERROR),
        kInitFailedError));
  }

  // Check the timeout.
  int64_t timeout_millis = params->params.timeout_millis;
  if (timeout_millis <= 0) {
    return RespondNow(ErrorWithArguments(
        Receive::Results::Create(STATUS_INVALIDREQUEST),
        kInvalidTimeoutError));
  }
  if (timeout_millis > kMaxReceiveTimeout)
    timeout_millis = kMaxReceiveTimeout;

  // Start receiving.
  api->StartReceive(extension_id(), params->params);
  return RespondNow(ArgumentList(Receive::Results::Create(STATUS_SUCCESS)));
}

ExtensionFunction::ResponseAction AudioModemStopReceiveFunction::Run() {
  scoped_ptr<StopReceive::Params> params(StopReceive::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Status status = AudioModemAPI::GetFactoryInstance()->Get(browser_context())
      ->StopReceive(extension_id(), AudioTypeForBand(params->band));
  return RespondNow(ArgumentList(StopReceive::Results::Create(status)));
}

}  // namespace extensions
