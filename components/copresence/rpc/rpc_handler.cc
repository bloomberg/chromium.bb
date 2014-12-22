// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/rpc/rpc_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

// TODO(ckehoe): time.h includes windows.h, which #defines DeviceCapabilities
// to DeviceCapabilitiesW. This breaks the pb.h headers below. For now,
// we fix this with an #undef.
#include "base/time/time.h"
#if defined(OS_WIN)
#undef DeviceCapabilities
#endif

#include "components/copresence/copresence_state_impl.h"
#include "components/copresence/copresence_switches.h"
#include "components/copresence/handlers/directive_handler.h"
#include "components/copresence/handlers/gcm_handler.h"
#include "components/copresence/proto/codes.pb.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/public/copresence_constants.h"
#include "components/copresence/public/copresence_delegate.h"
#include "components/copresence/rpc/http_post.h"
#include "net/http/http_status_code.h"

// TODO(ckehoe): Return error messages for bad requests.

namespace copresence {

using google::protobuf::MessageLite;
using google::protobuf::RepeatedPtrField;

const char RpcHandler::kReportRequestRpcName[] = "report";

namespace {

const int kTokenLoggingSuffix = 5;
const int kInvalidTokenExpiryTimeMs = 10 * 60 * 1000;  // 10 minutes.
const int kMaxInvalidTokens = 10000;
const char kRegisterDeviceRpcName[] = "registerdevice";
const char kDefaultCopresenceServer[] =
    "https://www.googleapis.com/copresence/v2/copresence";

// UrlSafe is defined as:
// '/' represented by a '_' and '+' represented by a '-'
// TODO(rkc): Move this to the wrapper.
std::string ToUrlSafe(std::string token) {
  base::ReplaceChars(token, "+", "-", &token);
  base::ReplaceChars(token, "/", "_", &token);
  return token;
}


// Logging

// Checks for a copresence error. If there is one, logs it and returns true.
bool IsErrorStatus(const Status& status) {
  if (status.code() != OK) {
    LOG(ERROR) << "Copresence error code " << status.code()
               << (status.message().empty() ? "" : ": " + status.message());
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
  bool result = IsErrorStatus(response.header().status());

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

const std::string LoggingStrForToken(const std::string& auth_token) {
  if (auth_token.empty())
    return "anonymous";

  std::string token_suffix = auth_token.substr(
      auth_token.length() - kTokenLoggingSuffix, kTokenLoggingSuffix);
  return base::StringPrintf("token ...%s", token_suffix.c_str());
}


// Request construction
// TODO(ckehoe): Move these into a separate file?

template <typename T>
BroadcastScanConfiguration GetBroadcastScanConfig(const T& msg) {
  if (msg.has_token_exchange_strategy() &&
      msg.token_exchange_strategy().has_broadcast_scan_configuration()) {
    return msg.token_exchange_strategy().broadcast_scan_configuration();
  }
  return BROADCAST_SCAN_CONFIGURATION_UNKNOWN;
}

scoped_ptr<DeviceState> GetDeviceCapabilities(const ReportRequest& request) {
  scoped_ptr<DeviceState> state(new DeviceState);

  TokenTechnology* ultrasound =
      state->mutable_capabilities()->add_token_technology();
  ultrasound->set_medium(AUDIO_ULTRASOUND_PASSBAND);
  ultrasound->add_instruction_type(TRANSMIT);
  ultrasound->add_instruction_type(RECEIVE);

  TokenTechnology* audible =
      state->mutable_capabilities()->add_token_technology();
  audible->set_medium(AUDIO_AUDIBLE_DTMF);
  audible->add_instruction_type(TRANSMIT);
  audible->add_instruction_type(RECEIVE);

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

void AddTokenToRequest(const AudioToken& token, ReportRequest* request) {
  TokenObservation* token_observation =
      request->mutable_update_signals_request()->add_token_observation();
  token_observation->set_token_id(ToUrlSafe(token.token));

  TokenSignals* signals = token_observation->add_signals();
  signals->set_medium(token.audible ? AUDIO_AUDIBLE_DTMF
                                    : AUDIO_ULTRASOUND_PASSBAND);
  signals->set_observed_time_millis(base::Time::Now().ToJsTime());
}

}  // namespace


// Public functions.

RpcHandler::RpcHandler(CopresenceDelegate* delegate,
                       CopresenceStateImpl* state,
                       DirectiveHandler* directive_handler,
                       GCMHandler* gcm_handler,
                       const PostCallback& server_post_callback)
    : delegate_(delegate),
      state_(state),
      directive_handler_(directive_handler),
      gcm_handler_(gcm_handler),
      server_post_callback_(server_post_callback),
      invalid_audio_token_cache_(
          base::TimeDelta::FromMilliseconds(kInvalidTokenExpiryTimeMs),
          kMaxInvalidTokens) {
    DCHECK(delegate_);
    DCHECK(directive_handler_);
    // |gcm_handler_| is optional.

    if (server_post_callback_.is_null()) {
      server_post_callback_ =
          base::Bind(&RpcHandler::SendHttpPost, base::Unretained(this));
    }

    if (gcm_handler_) {
      gcm_handler_->GetGcmId(
          base::Bind(&RpcHandler::RegisterGcmId, base::Unretained(this)));
    }
  }

RpcHandler::~RpcHandler() {
  // Do not use |directive_handler_| or |gcm_handler_| here.
  // They will already have been destructed.
  for (HttpPost* post : pending_posts_)
    delete post;
}

void RpcHandler::SendReportRequest(scoped_ptr<ReportRequest> request,
                                   const std::string& app_id,
                                   const std::string& auth_token,
                                   const StatusCallback& status_callback) {
  DCHECK(request.get());

  // Check that we have a "device" registered for this auth token.
  bool queue_request = true;
  const auto& registration = device_id_by_auth_token_.find(auth_token);
  if (registration == device_id_by_auth_token_.end()) {
    // Not registered.
    RegisterForToken(auth_token);
  } else if (!registration->second.empty()) {
    // Registration complete.
    queue_request = false;
  }

  // We're not registered, or registration is in progress.
  if (queue_request) {
    pending_requests_queue_.push_back(new PendingRequest(
        request.Pass(), app_id, auth_token, status_callback));
    return;
  }

  DVLOG(3) << "Sending ReportRequest to server.";

  // If we are unpublishing or unsubscribing, we need to stop those publish or
  // subscribes right away, we don't need to wait for the server to tell us.
  ProcessRemovedOperations(*request);

  request->mutable_update_signals_request()->set_allocated_state(
      GetDeviceCapabilities(*request).release());

  AddPlayingTokens(request.get());

  SendServerRequest(kReportRequestRpcName,
                    registration->second,
                    app_id,
                    auth_token,
                    request.Pass(),
                    // On destruction, this request will be cancelled.
                    base::Bind(&RpcHandler::ReportResponseHandler,
                               base::Unretained(this),
                               status_callback));
}

void RpcHandler::ReportTokens(const std::vector<AudioToken>& tokens) {
  DCHECK(!tokens.empty());

  if (device_id_by_auth_token_.empty()) {
    VLOG(2) << "Skipping token reporting because no device IDs are registered";
    return;
  }

  // Construct the ReportRequest.
  ReportRequest request;
  for (const AudioToken& token : tokens) {
    if (invalid_audio_token_cache_.HasKey(ToUrlSafe(token.token)))
      continue;
    DVLOG(3) << "Sending token " << token.token << " to server under "
             << device_id_by_auth_token_.size() << " device ID(s)";
    AddTokenToRequest(token, &request);
  }

  // Report under all active tokens.
  for (const auto& registration : device_id_by_auth_token_) {
    SendReportRequest(make_scoped_ptr(new ReportRequest(request)),
                      registration.first);
  }
}


// Private functions.

RpcHandler::PendingRequest::PendingRequest(scoped_ptr<ReportRequest> report,
                                           const std::string& app_id,
                                           const std::string& auth_token,
                                           const StatusCallback& callback)
    : report(report.Pass()),
      app_id(app_id),
      auth_token(auth_token),
      callback(callback) {}

RpcHandler::PendingRequest::~PendingRequest() {}

void RpcHandler::RegisterForToken(const std::string& auth_token) {
  DVLOG(2) << "Sending " << LoggingStrForToken(auth_token)
           << " registration to server.";

  scoped_ptr<RegisterDeviceRequest> request(new RegisterDeviceRequest);

  // Add a GCM ID for authenticated registration, if we have one.
  if (auth_token.empty() || gcm_id_.empty()) {
    request->mutable_push_service()->set_service(PUSH_SERVICE_NONE);
  } else {
    DVLOG(2) << "Registering GCM ID with " << LoggingStrForToken(auth_token);
    request->mutable_push_service()->set_service(GCM);
    request->mutable_push_service()->mutable_gcm_registration()
        ->set_device_token(gcm_id_);
  }

  // Only identify as a Chrome device if we're in anonymous mode.
  // Authenticated calls come from a "GAIA device".
  if (auth_token.empty()) {
    Identity* identity =
        request->mutable_device_identifiers()->mutable_registrant();
    identity->set_type(CHROME);
    identity->set_chrome_id(base::GenerateGUID());

    // Since we're generating a new "Chrome ID" here,
    // we need to make sure this isn't a duplicate registration.
    DCHECK_EQ(0u, device_id_by_auth_token_.count(std::string()))
        << "Attempted anonymous re-registration";
  }

  bool gcm_pending = !auth_token.empty() && gcm_handler_ && gcm_id_.empty();
  SendServerRequest(
      kRegisterDeviceRpcName,
      // This will have the side effect of populating an empty device ID
      // for this auth token in the map. This is what we want,
      // to mark registration as being in progress.
      device_id_by_auth_token_[auth_token],
      std::string(),  // app ID
      auth_token,
      request.Pass(),
      base::Bind(&RpcHandler::RegisterResponseHandler,
                 // On destruction, this request will be cancelled.
                 base::Unretained(this),
                 auth_token,
                 gcm_pending));
}

void RpcHandler::ProcessQueuedRequests(const std::string& auth_token) {
  // Track requests that are not on this auth token.
  ScopedVector<PendingRequest> still_pending_requests;

  // If there is no device ID for this auth token, registration failed.
  bool registration_failed =
      (device_id_by_auth_token_.count(auth_token) == 0);

  // We momentarily take ownership of all the pointers in the queue.
  // They are either deleted here or passed on to a new queue.
  for (PendingRequest* request : pending_requests_queue_) {
    if (request->auth_token == auth_token) {
      if (registration_failed) {
        request->callback.Run(FAIL);
      } else {
        SendReportRequest(request->report.Pass(),
                          request->app_id,
                          request->auth_token,
                          request->callback);
      }
      delete request;
    } else {
      // The request is on a different auth token.
      still_pending_requests.push_back(request);
    }
  }

  // Only keep the requests that weren't processed.
  // All the pointers in the queue are now spoken for.
  pending_requests_queue_.weak_clear();
  pending_requests_queue_ = still_pending_requests.Pass();
}

void RpcHandler::SendReportRequest(scoped_ptr<ReportRequest> request,
                                   const std::string& auth_token) {
  SendReportRequest(request.Pass(),
                    std::string(),
                    auth_token,
                    StatusCallback());
}

// Store a GCM ID and send it to the server if needed. The constructor passes
// this callback to the GCMHandler to receive the ID whenever it's ready.
// It may be returned immediately, if the ID is cached, or require a server
// round-trip. This ID must then be passed along to the copresence server.
// There are a few ways this can happen for each auth token:
//
// 1. The GCM ID is available when we first register, and is passed along
//    with the RegisterDeviceRequest.
//
// 2. The GCM ID becomes available after the RegisterDeviceRequest has
//    completed. Then the loop in this function will invoke RegisterForToken()
//    again to pass on the ID.
//
// 3. The GCM ID becomes available after the RegisterDeviceRequest is sent,
//    but before it completes. In this case, the gcm_pending flag is passed
//    through to the RegisterResponseHandler, which invokes RegisterForToken()
//    again to pass on the ID. The loop here must skip pending registrations,
//    as the device ID will be empty.
//
// TODO(ckehoe): Add tests for these scenarios.
void RpcHandler::RegisterGcmId(const std::string& gcm_id) {
  gcm_id_ = gcm_id;
  if (!gcm_id.empty()) {
    for (const auto& registration : device_id_by_auth_token_) {
      const std::string& auth_token = registration.first;
      const std::string& device_id = registration.second;
      if (!auth_token.empty() && !device_id.empty())
        RegisterForToken(auth_token);
    }
  }
}

void RpcHandler::RegisterResponseHandler(
    const std::string& auth_token,
    bool gcm_pending,
    HttpPost* completed_post,
    int http_status_code,
    const std::string& response_data) {
  if (completed_post) {
    int elements_erased = pending_posts_.erase(completed_post);
    DCHECK_GT(elements_erased, 0);
    delete completed_post;
  }

  // Registration is no longer in progress.
  // If it was successful, we'll update below.
  device_id_by_auth_token_.erase(auth_token);

  RegisterDeviceResponse response;
  if (http_status_code != net::HTTP_OK) {
    // TODO(ckehoe): Retry registration if appropriate.
    LOG(ERROR) << LoggingStrForToken(auth_token)
               << " device registration failed";
  } else if (!response.ParseFromString(response_data)) {
    LOG(ERROR) << "Invalid RegisterDeviceResponse:\n" << response_data;
  } else if (!IsErrorStatus(response.header().status())) {
    const std::string& device_id = response.registered_device_id();
    DCHECK(!device_id.empty());
    device_id_by_auth_token_[auth_token] = device_id;
    DVLOG(2) << LoggingStrForToken(auth_token)
             << " device registration successful. Id: " << device_id;

    // If we have a GCM ID now, and didn't before, pass it on to the server.
    if (gcm_pending && !gcm_id_.empty())
      RegisterForToken(auth_token);
  }

  // Send or fail requests on this auth token.
  ProcessQueuedRequests(auth_token);
}

void RpcHandler::ReportResponseHandler(const StatusCallback& status_callback,
                                       HttpPost* completed_post,
                                       int http_status_code,
                                       const std::string& response_data) {
  if (completed_post) {
    int elements_erased = pending_posts_.erase(completed_post);
    DCHECK(elements_erased);
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

  for (const MessageResult& result :
      response.manage_messages_response().published_message_result()) {
    DVLOG(2) << "Published message with id " << result.published_message_id();
  }

  for (const SubscriptionResult& result :
      response.manage_subscriptions_response().subscription_result()) {
    DVLOG(2) << "Created subscription with id " << result.subscription_id();
  }

  if (response.has_update_signals_response()) {
    const UpdateSignalsResponse& update_response =
        response.update_signals_response();
    DispatchMessages(update_response.message());

    for (const Directive& directive : update_response.directive())
      directive_handler_->AddDirective(directive);

    for (const Token& token : update_response.token()) {
      state_->UpdateTokenStatus(token.id(), token.status());
      switch (token.status()) {
        case VALID:
          // TODO(rkc/ckehoe): Store the token in a |valid_token_cache_| with a
          // short TTL (like 10s) and send it up with every report request.
          // Then we'll still get messages while we're waiting to hear it again.
          VLOG(1) << "Got valid token " << token.id();
          break;
        case INVALID:
          DVLOG(3) << "Discarding invalid token " << token.id();
          invalid_audio_token_cache_.Add(token.id(), true);
          break;
        default:
          DVLOG(2) << "Token " << token.id() << " has status code "
                   << token.status();
      }
    }
  }

  // TODO(ckehoe): Return a more detailed status response.
  if (!status_callback.is_null())
    status_callback.Run(SUCCESS);
}

void RpcHandler::ProcessRemovedOperations(const ReportRequest& request) {
  // Remove unpublishes.
  if (request.has_manage_messages_request()) {
    for (const std::string& unpublish :
        request.manage_messages_request().id_to_unpublish()) {
      directive_handler_->RemoveDirectives(unpublish);
    }
  }

  // Remove unsubscribes.
  if (request.has_manage_subscriptions_request()) {
    for (const std::string& unsubscribe :
        request.manage_subscriptions_request().id_to_unsubscribe()) {
      directive_handler_->RemoveDirectives(unsubscribe);
    }
  }
}

void RpcHandler::AddPlayingTokens(ReportRequest* request) {
  const std::string& audible_token =
      directive_handler_->GetCurrentAudioToken(AUDIBLE);
  const std::string& inaudible_token =
      directive_handler_->GetCurrentAudioToken(INAUDIBLE);

  if (!audible_token.empty())
    AddTokenToRequest(AudioToken(audible_token, true), request);
  if (!inaudible_token.empty())
    AddTokenToRequest(AudioToken(inaudible_token, false), request);
}

void RpcHandler::DispatchMessages(
    const RepeatedPtrField<SubscribedMessage>& messages) {
  if (messages.size() == 0)
    return;

  // Index the messages by subscription id.
  std::map<std::string, std::vector<Message>> messages_by_subscription;
  DVLOG(3) << "Dispatching " << messages.size() << " messages";
  for (const SubscribedMessage& message : messages) {
    for (const std::string& subscription_id : message.subscription_id()) {
      messages_by_subscription[subscription_id].push_back(
          message.published_message());
    }
  }

  // Send the messages for each subscription.
  for (const auto& map_entry : messages_by_subscription) {
    // TODO(ckehoe): Once we have the app ID from the server, we need to pass
    // it in here and get rid of the app id registry from the main API class.
    const std::string& subscription = map_entry.first;
    const std::vector<Message>& messages = map_entry.second;
    delegate_->HandleMessages(std::string(), subscription, messages);
  }
}

// TODO(ckehoe): Pass in the version string and
// group this with the local functions up top.
RequestHeader* RpcHandler::CreateRequestHeader(
    const std::string& client_name,
    const std::string& device_id) const {
  RequestHeader* header = new RequestHeader;

  header->set_allocated_framework_version(CreateVersion(
      "Chrome", delegate_->GetPlatformVersionString()));
  if (!client_name.empty()) {
    header->set_allocated_client_version(
        CreateVersion(client_name, std::string()));
  }
  header->set_current_time_millis(base::Time::Now().ToJsTime());
  if (!device_id.empty())
    header->set_registered_device_id(device_id);

  DeviceFingerprint* fingerprint = new DeviceFingerprint;
  fingerprint->set_platform_version(delegate_->GetPlatformVersionString());
  fingerprint->set_type(CHROME_PLATFORM_TYPE);
  header->set_allocated_device_fingerprint(fingerprint);

  return header;
}

template <class T>
void RpcHandler::SendServerRequest(
    const std::string& rpc_name,
    const std::string& device_id,
    const std::string& app_id,
    const std::string& auth_token,
    scoped_ptr<T> request,
    const PostCleanupCallback& response_handler) {
  request->set_allocated_header(CreateRequestHeader(app_id, device_id));
  server_post_callback_.Run(delegate_->GetRequestContext(),
                            rpc_name,
                            delegate_->GetAPIKey(app_id),
                            auth_token,
                            make_scoped_ptr<MessageLite>(request.release()),
                            response_handler);
}

void RpcHandler::SendHttpPost(net::URLRequestContextGetter* url_context_getter,
                              const std::string& rpc_name,
                              const std::string& api_key,
                              const std::string& auth_token,
                              scoped_ptr<MessageLite> request_proto,
                              const PostCleanupCallback& callback) {
  // Create the base URL to call.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const std::string copresence_server_host =
      command_line->HasSwitch(switches::kCopresenceServer) ?
      command_line->GetSwitchValueASCII(switches::kCopresenceServer) :
      kDefaultCopresenceServer;

  // Create the request and keep a pointer until it completes.
  HttpPost* http_post = new HttpPost(
      url_context_getter,
      copresence_server_host,
      rpc_name,
      api_key,
      auth_token,
      command_line->GetSwitchValueASCII(switches::kCopresenceTracingToken),
      *request_proto);

  http_post->Start(base::Bind(callback, http_post));
  pending_posts_.insert(http_post);
}

}  // namespace copresence
