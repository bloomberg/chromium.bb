// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/rpc/rpc_handler.h"

#include <map>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/copresence/copresence_switches.h"
#include "components/copresence/handlers/directive_handler.h"
#include "components/copresence/proto/codes.pb.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/public/copresence_client_delegate.h"
#include "net/http/http_status_code.h"

// TODO(ckehoe): Return error messages for bad requests.

namespace copresence {

using google::protobuf::MessageLite;
using google::protobuf::RepeatedPtrField;

const char RpcHandler::kReportRequestRpcName[] = "report";

namespace {

// UrlSafe is defined as:
// '/' represented by a '_' and '+' represented by a '-'
// TODO(rkc): Move this to the wrapper.
std::string ToUrlSafe(std::string token) {
  base::ReplaceChars(token, "+", "-", &token);
  base::ReplaceChars(token, "/", "_", &token);
  return token;
}

const int kInvalidTokenExpiryTimeMs = 10 * 60 * 1000;  // 10 minutes.
const int kMaxInvalidTokens = 10000;
const char kRegisterDeviceRpcName[] = "registerdevice";
const char kDefaultCopresenceServer[] =
    "https://www.googleapis.com/copresence/v2/copresence";

// Logging

// Checks for a copresence error. If there is one, logs it and returns true.
bool CopresenceErrorLogged(const Status& status) {
  if (status.code() != OK) {
    LOG(ERROR) << "Copresence error code " << status.code()
               << (status.message().empty() ? std::string() :
                  ": " + status.message());
  }
  return status.code() != OK;
}

void LogIfErrorStatus(const util::error::Code& code,
                      const std::string& context) {
  LOG_IF(ERROR, code != util::error::OK)
      << context << " error " << code << ". See "
      << "cs/google3/util/task/codes.proto for more info.";
}

// If any errors occurred, logs them and returns true.
bool ReportErrorLogged(const ReportResponse& response) {
  bool result = CopresenceErrorLogged(response.header().status());

  // The Report fails or succeeds as a unit. If any responses had errors,
  // the header will too. Thus we don't need to propagate individual errors.
  if (response.has_update_signals_response())
    LogIfErrorStatus(response.update_signals_response().status(), "Update");
  if (response.has_manage_messages_response())
    LogIfErrorStatus(response.manage_messages_response().status(), "Publish");
  if (response.has_manage_subscriptions_response()) {
    LogIfErrorStatus(response.manage_subscriptions_response().status(),
                     "Subscribe");
  }

  return result;
}

// Request construction

template <typename T>
BroadcastScanConfiguration GetBroadcastScanConfig(const T& msg) {
  if (msg.has_token_exchange_strategy() &&
      msg.token_exchange_strategy().has_broadcast_scan_configuration()) {
    return msg.token_exchange_strategy().broadcast_scan_configuration();
  }
  return BROADCAST_SCAN_CONFIGURATION_UNKNOWN;
}

// This method will extract token exchange strategies
// from the publishes and subscribes in a report request.
// TODO(ckehoe): Delete this when the server supports
// BroadcastScanConfiguration.
BroadcastScanConfiguration ExtractTokenExchangeStrategy(
    const ReportRequest& request) {
  bool broadcast_only = false;
  bool scan_only = false;

  // Strategies for publishes.
  if (request.has_manage_messages_request()) {
    const RepeatedPtrField<PublishedMessage> messages =
        request.manage_messages_request().message_to_publish();
    for (int i = 0; i < messages.size(); ++i) {
      BroadcastScanConfiguration config =
          GetBroadcastScanConfig(messages.Get(i));
      broadcast_only = broadcast_only || config == BROADCAST_ONLY;
      scan_only = scan_only || config == SCAN_ONLY;
      if (config == BROADCAST_AND_SCAN || (broadcast_only && scan_only))
        return BROADCAST_AND_SCAN;
    }
  }

  // Strategies for subscriptions.
  if (request.has_manage_subscriptions_request()) {
    const RepeatedPtrField<Subscription> messages =
        request.manage_subscriptions_request().subscription();
    for (int i = 0; i < messages.size(); ++i) {
      BroadcastScanConfiguration config =
          GetBroadcastScanConfig(messages.Get(i));
      broadcast_only = broadcast_only || config == BROADCAST_ONLY;
      scan_only = scan_only || config == SCAN_ONLY;
      if (config == BROADCAST_AND_SCAN || (broadcast_only && scan_only))
        return BROADCAST_AND_SCAN;
    }
  }

  if (broadcast_only)
    return BROADCAST_ONLY;
  if (scan_only)
    return SCAN_ONLY;

  // If nothing else is specified, default to both broadcast and scan.
  return BROADCAST_AND_SCAN;
}

// TODO(rkc): Fix this hack once the server supports setting strategies per
// operation.
bool ExtractIsAudibleStrategy(const ReportRequest& request) {
  if (request.has_manage_messages_request()) {
    const RepeatedPtrField<PublishedMessage> messages =
        request.manage_messages_request().message_to_publish();
    for (int i = 0; i < messages.size(); ++i) {
      const PublishedMessage& msg = messages.Get(i);
      if (msg.has_token_exchange_strategy() &&
          msg.token_exchange_strategy().has_use_audible() &&
          msg.token_exchange_strategy().use_audible()) {
        return true;
      }
    }
  }
  return false;
}

scoped_ptr<DeviceState> GetDeviceCapabilities(const ReportRequest& request) {
  scoped_ptr<DeviceState> state(new DeviceState);

// TODO(ckehoe): Currently this code causes a linker error on Windows.
#ifndef OS_WIN
  TokenTechnology* token_technology =
      state->mutable_capabilities()->add_token_technology();
  token_technology->set_medium(AUDIO_ULTRASOUND_PASSBAND);
  if (ExtractIsAudibleStrategy(request))
    token_technology->set_medium(AUDIO_AUDIBLE_DTMF);

  BroadcastScanConfiguration config =
      ExtractTokenExchangeStrategy(request);
  if (config == BROADCAST_ONLY || config == BROADCAST_AND_SCAN)
    token_technology->add_instruction_type(TRANSMIT);
  if (config == SCAN_ONLY || config == BROADCAST_AND_SCAN)
    token_technology->add_instruction_type(RECEIVE);
#endif

  return state.Pass();
}

// TODO(ckehoe): We're keeping this code in a separate function for now
// because we get a version string from Chrome, but the proto expects
// an int64 version. We should probably change the version proto
// to handle a more detailed version.
ClientVersion* CreateVersion(const std::string& client,
                             const std::string& version_name) {
  ClientVersion* version = new ClientVersion;

  version->set_client(client);
  version->set_version_name(version_name);

  return version;
}

}  // namespace

// Public methods

RpcHandler::RpcHandler(CopresenceClientDelegate* delegate)
    : delegate_(delegate),
      invalid_audio_token_cache_(
          base::TimeDelta::FromMilliseconds(kInvalidTokenExpiryTimeMs),
          kMaxInvalidTokens),
      server_post_callback_(base::Bind(&RpcHandler::SendHttpPost,
                                       base::Unretained(this))) {}

RpcHandler::~RpcHandler() {
  for (std::set<HttpPost*>::iterator post = pending_posts_.begin();
       post != pending_posts_.end(); ++post) {
    delete *post;
  }

  if (delegate_ && delegate_->GetWhispernetClient()) {
    delegate_->GetWhispernetClient()->RegisterTokensCallback(
        WhispernetClient::TokensCallback());
  }
}

void RpcHandler::Initialize(const SuccessCallback& init_done_callback) {
  scoped_ptr<RegisterDeviceRequest> request(new RegisterDeviceRequest);
  DCHECK(device_id_.empty());

  request->mutable_push_service()->set_service(PUSH_SERVICE_NONE);
  Identity* identity =
      request->mutable_device_identifiers()->mutable_registrant();
  identity->set_type(CHROME);
  identity->set_chrome_id(base::GenerateGUID());
  SendServerRequest(
      kRegisterDeviceRpcName,
      std::string(),
      request.Pass(),
      base::Bind(&RpcHandler::RegisterResponseHandler,
                 // On destruction, this request will be cancelled.
                 base::Unretained(this),
                 init_done_callback));
}

void RpcHandler::SendReportRequest(scoped_ptr<ReportRequest> request) {
  SendReportRequest(request.Pass(), std::string(), StatusCallback());
}

void RpcHandler::SendReportRequest(scoped_ptr<ReportRequest> request,
                                   const std::string& app_id,
                                   const StatusCallback& status_callback) {
  DCHECK(request.get());
  DCHECK(!device_id_.empty())
      << "RpcHandler::Initialize() must complete successfully "
      << "before other RpcHandler methods are called.";

  DVLOG(3) << "Sending report request to server.";

  request->mutable_update_signals_request()->set_allocated_state(
      GetDeviceCapabilities(*request).release());
  SendServerRequest(kReportRequestRpcName,
                    app_id,
                    request.Pass(),
                    // On destruction, this request will be cancelled.
                    base::Bind(&RpcHandler::ReportResponseHandler,
                               base::Unretained(this),
                               status_callback));
}

void RpcHandler::ReportTokens(const std::vector<FullToken>& tokens) {
  DCHECK(!tokens.empty());

  scoped_ptr<ReportRequest> request(new ReportRequest);
  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string& token = ToUrlSafe(tokens[i].token);
    if (invalid_audio_token_cache_.HasKey(token))
      continue;

    DVLOG(3) << "Sending token " << token << " to server.";

    TokenObservation* token_observation =
        request->mutable_update_signals_request()->add_token_observation();
    token_observation->set_token_id(token);

    TokenSignals* signals = token_observation->add_signals();
    signals->set_medium(tokens[i].audible ? AUDIO_AUDIBLE_DTMF
                                          : AUDIO_ULTRASOUND_PASSBAND);
    signals->set_observed_time_millis(base::Time::Now().ToJsTime());
  }
  SendReportRequest(request.Pass());
}

void RpcHandler::ConnectToWhispernet() {
  WhispernetClient* whispernet_client = delegate_->GetWhispernetClient();

  // |directive_handler_| will be destructed with us, so unretained is safe.
  directive_handler_.reset(new DirectiveHandler);
  directive_handler_->Initialize(
      base::Bind(&WhispernetClient::DecodeSamples,
                 base::Unretained(whispernet_client)),
      base::Bind(&RpcHandler::AudioDirectiveListToWhispernetConnector,
                 base::Unretained(this)));

  whispernet_client->RegisterTokensCallback(
      base::Bind(&RpcHandler::ReportTokens,
                 // On destruction, this callback will be disconnected.
                 base::Unretained(this)));
}

// Private methods

void RpcHandler::RegisterResponseHandler(
    const SuccessCallback& init_done_callback,
    HttpPost* completed_post,
    int http_status_code,
    const std::string& response_data) {
  if (completed_post) {
    DCHECK(pending_posts_.erase(completed_post));
    delete completed_post;
  }

  if (http_status_code != net::HTTP_OK) {
    init_done_callback.Run(false);
    return;
  }

  RegisterDeviceResponse response;
  if (!response.ParseFromString(response_data)) {
    LOG(ERROR) << "Invalid RegisterDeviceResponse:\n" << response_data;
    init_done_callback.Run(false);
    return;
  }

  if (CopresenceErrorLogged(response.header().status()))
    return;
  device_id_ = response.registered_device_id();
  DCHECK(!device_id_.empty());
  DVLOG(2) << "Device registration successful: id " << device_id_;
  init_done_callback.Run(true);
}

void RpcHandler::ReportResponseHandler(const StatusCallback& status_callback,
                                       HttpPost* completed_post,
                                       int http_status_code,
                                       const std::string& response_data) {
  if (completed_post) {
    DCHECK(pending_posts_.erase(completed_post));
    delete completed_post;
  }

  if (http_status_code != net::HTTP_OK) {
    if (!status_callback.is_null())
      status_callback.Run(FAIL);
    return;
  }

  DVLOG(3) << "Received ReportResponse.";
  ReportResponse response;
  if (!response.ParseFromString(response_data)) {
    LOG(ERROR) << "Invalid ReportResponse";
    if (!status_callback.is_null())
      status_callback.Run(FAIL);
    return;
  }

  if (ReportErrorLogged(response)) {
    if (!status_callback.is_null())
      status_callback.Run(FAIL);
    return;
  }

  const RepeatedPtrField<MessageResult>& message_results =
      response.manage_messages_response().published_message_result();
  for (int i = 0; i < message_results.size(); ++i) {
    DVLOG(2) << "Published message with id "
             << message_results.Get(i).published_message_id();
  }

  const RepeatedPtrField<SubscriptionResult>& subscription_results =
      response.manage_subscriptions_response().subscription_result();
  for (int i = 0; i < subscription_results.size(); ++i) {
    DVLOG(2) << "Created subscription with id "
             << subscription_results.Get(i).subscription_id();
  }

  if (response.has_update_signals_response()) {
    const UpdateSignalsResponse& update_response =
        response.update_signals_response();
    DispatchMessages(update_response.message());

    if (directive_handler_.get()) {
      for (int i = 0; i < update_response.directive_size(); ++i)
        directive_handler_->AddDirective(update_response.directive(i));
    } else {
      DVLOG(1) << "No directive handler.";
    }

    const RepeatedPtrField<Token>& tokens = update_response.token();
    for (int i = 0; i < tokens.size(); ++i) {
      switch (tokens.Get(i).status()) {
        case VALID:
          // TODO(rkc/ckehoe): Store the token in a |valid_token_cache_| with a
          // short TTL (like 10s) and send it up with every report request.
          // Then we'll still get messages while we're waiting to hear it again.
          VLOG(1) << "Got valid token " << tokens.Get(i).id();
          break;
        case INVALID:
          DVLOG(3) << "Discarding invalid token " << tokens.Get(i).id();
          invalid_audio_token_cache_.Add(tokens.Get(i).id(), true);
          break;
        default:
          DVLOG(2) << "Token " << tokens.Get(i).id() << " has status code "
                   << tokens.Get(i).status();
      }
    }
  }

  // TODO(ckehoe): Return a more detailed status response.
  if (!status_callback.is_null())
    status_callback.Run(SUCCESS);
}

void RpcHandler::DispatchMessages(
    const RepeatedPtrField<SubscribedMessage>& messages) {
  if (messages.size() == 0)
    return;

  // Index the messages by subscription id.
  std::map<std::string, std::vector<Message> > messages_by_subscription;
  DVLOG(3) << "Dispatching " << messages.size() << " messages";
  for (int m = 0; m < messages.size(); ++m) {
    const RepeatedPtrField<std::string>& subscription_ids =
        messages.Get(m).subscription_id();
    for (int s = 0; s < subscription_ids.size(); ++s) {
      messages_by_subscription[subscription_ids.Get(s)].push_back(
          messages.Get(m).published_message());
    }
  }

  // Send the messages for each subscription.
  for (std::map<std::string, std::vector<Message> >::const_iterator
           subscription = messages_by_subscription.begin();
       subscription != messages_by_subscription.end();
       ++subscription) {
    // TODO(ckehoe): Once we have the app ID from the server, we need to pass
    // it in here and get rid of the app id registry from the main API class.
    delegate_->HandleMessages("", subscription->first, subscription->second);
  }
}

RequestHeader* RpcHandler::CreateRequestHeader(
    const std::string& client_name) const {
  RequestHeader* header = new RequestHeader;

  header->set_allocated_framework_version(
      CreateVersion("Chrome", delegate_->GetPlatformVersionString()));
  if (!client_name.empty()) {
    header->set_allocated_client_version(
        CreateVersion(client_name, std::string()));
  }
  header->set_current_time_millis(base::Time::Now().ToJsTime());
  header->set_registered_device_id(device_id_);

  return header;
}

template <class T>
void RpcHandler::SendServerRequest(
    const std::string& rpc_name,
    const std::string& app_id,
    scoped_ptr<T> request,
    const PostCleanupCallback& response_handler) {
  request->set_allocated_header(CreateRequestHeader(app_id));
  server_post_callback_.Run(delegate_->GetRequestContext(),
                            rpc_name,
                            make_scoped_ptr<MessageLite>(request.release()),
                            response_handler);
}

void RpcHandler::SendHttpPost(net::URLRequestContextGetter* url_context_getter,
                              const std::string& rpc_name,
                              scoped_ptr<MessageLite> request_proto,
                              const PostCleanupCallback& callback) {
  // Create the base URL to call.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const std::string copresence_server_host =
      command_line->HasSwitch(switches::kCopresenceServer) ?
      command_line->GetSwitchValueASCII(switches::kCopresenceServer) :
      kDefaultCopresenceServer;

  // Create the request and keep a pointer until it completes.
  const std::string& tracing_token =
      command_line->GetSwitchValueASCII(switches::kCopresenceTracingToken);
  HttpPost* http_post = new HttpPost(url_context_getter,
                                     copresence_server_host,
                                     rpc_name,
                                     tracing_token,
                                     *request_proto);
  http_post->Start(base::Bind(callback, http_post));
  pending_posts_.insert(http_post);
}

void RpcHandler::AudioDirectiveListToWhispernetConnector(
    const std::string& token,
    bool audible,
    const WhispernetClient::SamplesCallback& samples_callback) {
  WhispernetClient* whispernet_client = delegate_->GetWhispernetClient();
  if (whispernet_client) {
    whispernet_client->RegisterSamplesCallback(samples_callback);
    whispernet_client->EncodeToken(token, audible);
  }
}

}  // namespace copresence
