// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/copresence_private/copresence_private_api.h"

#include <map>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "chrome/browser/copresence/chrome_whispernet_client.h"
#include "chrome/common/extensions/api/copresence_private.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/audio_bus.h"

using audio_modem::WhispernetClient;
using content::BrowserThread;

namespace {

// TODO(ckehoe): Move these globals into a proper CopresencePrivateService.

base::LazyInstance<std::map<std::string, WhispernetClient*>>
g_whispernet_clients = LAZY_INSTANCE_INITIALIZER;

// To avoid concurrency issues, only use this on the UI thread.
static bool g_initialized = false;

WhispernetClient* GetWhispernetClient(const std::string& id) {
  WhispernetClient* client = g_whispernet_clients.Get()[id];
  DCHECK(client);
  return client;
}

void RunInitCallback(WhispernetClient* client, bool status) {
  DCHECK(client);
  audio_modem::SuccessCallback init_callback =
      client->GetInitializedCallback();
  if (!init_callback.is_null())
    init_callback.Run(status);
}

}  // namespace

namespace extensions {

namespace SendFound = api::copresence_private::SendFound;
namespace SendSamples = api::copresence_private::SendSamples;
namespace SendInitialized = api::copresence_private::SendInitialized;

namespace copresence_private {

const std::string RegisterWhispernetClient(WhispernetClient* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (g_initialized)
    RunInitCallback(client, true);

  std::string id = base::GenerateGUID();
  g_whispernet_clients.Get()[id] = client;

  return id;
}

}  // namespace copresence_private

// Copresence Private functions.

// CopresenceSendFoundFunction implementation:
ExtensionFunction::ResponseAction CopresencePrivateSendFoundFunction::Run() {
  scoped_ptr<SendFound::Params> params(SendFound::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WhispernetClient* whispernet_client = GetWhispernetClient(params->client_id);
  if (whispernet_client->GetTokensCallback().is_null())
    return RespondNow(NoArguments());

  std::vector<audio_modem::AudioToken> tokens;
  for (size_t i = 0; i < params->tokens.size(); ++i) {
    tokens.push_back(audio_modem::AudioToken(params->tokens[i]->token,
                                            params->tokens[i]->audible));
  }
  whispernet_client->GetTokensCallback().Run(tokens);
  return RespondNow(NoArguments());
}

// CopresenceSendEncodedFunction implementation:
ExtensionFunction::ResponseAction CopresencePrivateSendSamplesFunction::Run() {
  scoped_ptr<SendSamples::Params> params(SendSamples::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WhispernetClient* whispernet_client = GetWhispernetClient(params->client_id);
  if (whispernet_client->GetSamplesCallback().is_null())
    return RespondNow(NoArguments());

  scoped_refptr<media::AudioBusRefCounted> samples =
      media::AudioBusRefCounted::Create(1,  // Mono
                                        params->samples.size() / sizeof(float));

  memcpy(samples->channel(0), vector_as_array(&params->samples),
         params->samples.size());

  whispernet_client->GetSamplesCallback().Run(
      params->token.audible ? audio_modem::AUDIBLE : audio_modem::INAUDIBLE,
      params->token.token, samples);
  return RespondNow(NoArguments());
}

// CopresenceSendInitializedFunction implementation:
ExtensionFunction::ResponseAction
CopresencePrivateSendInitializedFunction::Run() {
  scoped_ptr<SendInitialized::Params> params(
      SendInitialized::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (params->success)
    g_initialized = true;

  DVLOG(2) << "Notifying " << g_whispernet_clients.Get().size()
           << " clients that initialization is complete.";
  for (auto client_entry : g_whispernet_clients.Get())
    RunInitCallback(client_entry.second, params->success);

  return RespondNow(NoArguments());
}

}  // namespace extensions
