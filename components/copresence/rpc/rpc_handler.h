// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_
#define COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/copresence/proto/enums.pb.h"
#include "components/copresence/public/copresence_constants.h"
#include "components/copresence/public/copresence_delegate.h"
#include "components/copresence/timed_map.h"

namespace audio_modem {
struct AudioToken;
}

namespace copresence {

class CopresenceDelegate;
class CopresenceStateImpl;
class DirectiveHandler;
class GCMHandler;
class HttpPost;
class ReportRequest;
class RequestHeader;
class SubscribedMessage;

// This class currently handles all communication with the copresence server.
class RpcHandler {
 public:
  // A callback to indicate whether handler initialization succeeded.
  using SuccessCallback = base::Callback<void(bool)>;

  // An HttpPost::ResponseCallback along with an HttpPost object to be deleted.
  // Arguments:
  // HttpPost*: The handler should take ownership of (i.e. delete) this object.
  // int: The HTTP status code of the response.
  // string: The contents of the response.
  using PostCleanupCallback = base::Callback<void(HttpPost*,
                                                  int,
                                                  const std::string&)>;

  // Callback to allow tests to stub out HTTP POST behavior.
  // Arguments:
  // URLRequestContextGetter: Context for the HTTP POST request.
  // string: Name of the rpc to invoke. URL format: server.google.com/rpc_name
  // string: The API key to pass in the request.
  // string: The auth token to pass with the request.
  // MessageLite: Contents of POST request to be sent. This needs to be
  //     a (scoped) pointer to ease handling of the abstract MessageLite class.
  // PostCleanupCallback: Receives the response to the request.
  using PostCallback = base::Callback<void(
      net::URLRequestContextGetter*,
      const std::string&,
      const std::string&,
      const std::string&,
      scoped_ptr<google::protobuf::MessageLite>,
      const PostCleanupCallback&)>;

  // Report rpc name to send to Apiary.
  static const char kReportRequestRpcName[];

  // Constructor. |delegate| and |directive_handler|
  // are owned by the caller and must outlive the RpcHandler.
  // |server_post_callback| should be set only by tests.
  RpcHandler(CopresenceDelegate* delegate,
             CopresenceStateImpl* state,
             DirectiveHandler* directive_handler,
             GCMHandler* gcm_handler,
             const MessagesCallback& new_messages_callback,
             const PostCallback& server_post_callback = PostCallback());

  virtual ~RpcHandler();

  // Send a ReportRequest from a specific app, and get notified of completion.
  void SendReportRequest(scoped_ptr<ReportRequest> request,
                         const std::string& app_id,
                         const std::string& auth_token,
                         const StatusCallback& callback);

  // Report a set of tokens to the server for a given medium.
  // Uses all active auth tokens (if any).
  void ReportTokens(const std::vector<audio_modem::AudioToken>& tokens);

 private:
  // A queued ReportRequest along with its metadata.
  struct PendingRequest {
    PendingRequest(scoped_ptr<ReportRequest> report,
                   const std::string& app_id,
                   bool authenticated,
                   const StatusCallback& callback);
    ~PendingRequest();

    scoped_ptr<ReportRequest> report;
    const std::string app_id;
    const bool authenticated;
    const StatusCallback callback;
  };

  friend class RpcHandlerTest;

  // Before accepting any other calls, the server requires registration,
  // which is tied to the auth token (or lack thereof) used to call Report.
  void RegisterDevice(bool authenticated);

  // Device registration has completed. Send the requests that it was blocking.
  void ProcessQueuedRequests(bool authenticated);

  // Send a ReportRequest from Chrome itself, i.e. no app id.
  void ReportOnAllDevices(scoped_ptr<ReportRequest> request);

  // Store a GCM ID and send it to the server if needed.
  void RegisterGcmId(const std::string& gcm_id);

  // Server call response handlers.
  void RegisterResponseHandler(bool authenticated,
                               bool gcm_pending,
                               HttpPost* completed_post,
                               int http_status_code,
                               const std::string& response_data);
  void ReportResponseHandler(const StatusCallback& status_callback,
                             HttpPost* completed_post,
                             int http_status_code,
                             const std::string& response_data);

  // If the request has any unpublish or unsubscribe operations, it removes
  // them from our directive handlers.
  void ProcessRemovedOperations(const ReportRequest& request);

  // Add all currently playing tokens to the update signals in this report
  // request. This ensures that the server doesn't keep issueing new tokens to
  // us when we're already playing valid tokens.
  void AddPlayingTokens(ReportRequest* request);

  void DispatchMessages(
      const google::protobuf::RepeatedPtrField<SubscribedMessage>&
      subscribed_messages);

  RequestHeader* CreateRequestHeader(const std::string& app_id,
                                     const std::string& device_id) const;

  // Post a request to the server. The request should be in proto format.
  template <class T>
  void SendServerRequest(const std::string& rpc_name,
                         const std::string& device_id,
                         const std::string& app_id,
                         bool authenticated,
                         scoped_ptr<T> request,
                         const PostCleanupCallback& response_handler);

  // Wrapper for the http post constructor. This is the default way
  // to contact the server, but it can be overridden for testing.
  void SendHttpPost(net::URLRequestContextGetter* url_context_getter,
                    const std::string& rpc_name,
                    const std::string& api_key,
                    const std::string& auth_token,
                    scoped_ptr<google::protobuf::MessageLite> request_proto,
                    const PostCleanupCallback& callback);

  // These belong to the caller.
  CopresenceDelegate* const delegate_;
  CopresenceStateImpl* state_;
  DirectiveHandler* const directive_handler_;
  GCMHandler* const gcm_handler_;

  MessagesCallback new_messages_callback_;
  PostCallback server_post_callback_;

  ScopedVector<PendingRequest> pending_requests_queue_;
  TimedMap<std::string, bool> invalid_audio_token_cache_;
  std::set<HttpPost*> pending_posts_;
  std::set<bool> pending_registrations_;
  std::string auth_token_;
  std::string gcm_id_;

  DISALLOW_COPY_AND_ASSIGN(RpcHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_
