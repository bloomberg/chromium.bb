// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_
#define COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "components/copresence/proto/enums.pb.h"
#include "components/copresence/public/copresence_client_delegate.h"
#include "components/copresence/public/whispernet_client.h"
#include "components/copresence/rpc/http_post.h"
#include "components/copresence/timed_map.h"

namespace copresence {

class DirectiveHandler;
class ReportRequest;
class RequestHeader;
class SubscribedMessage;

// This class currently handles all communication with the copresence server.
class RpcHandler {
 public:
  // A callback to indicate whether handler initialization succeeded.
  typedef base::Callback<void(bool)> SuccessCallback;

  // Report rpc name to send to Apiary.
  static const char kReportRequestRpcName[];

  // Constructor. |delegate| is owned by the caller,
  // and must be valid as long as the RpcHandler exists.
  explicit RpcHandler(CopresenceClientDelegate* delegate);

  virtual ~RpcHandler();

  // Clients must call this and wait for |init_done_callback|
  // to be called before invoking any other methods.
  void Initialize(const SuccessCallback& init_done_callback);

  // Send a report request
  void SendReportRequest(scoped_ptr<ReportRequest> request);
  void SendReportRequest(scoped_ptr<ReportRequest> request,
                         const std::string& app_id,
                         const StatusCallback& callback);

  // Report a set of tokens to the server for a given medium.
  void ReportTokens(const std::vector<FullToken>& tokens);

  // Create the directive handler and connect it to
  // the whispernet client specified by the delegate.
  void ConnectToWhispernet();

 private:
  // An HttpPost::ResponseCallback prepended with an HttpPost object
  // that needs to be deleted.
  typedef base::Callback<void(HttpPost*, int, const std::string&)>
      PostCleanupCallback;

  // Callback to allow tests to stub out HTTP POST behavior.
  // Arguments:
  // URLRequestContextGetter: Context for the HTTP POST request.
  // string: Name of the rpc to invoke. URL format: server.google.com/rpc_name
  // MessageLite: Contents of POST request to be sent. This needs to be
  //     a (scoped) pointer to ease handling of the abstract MessageLite class.
  // ResponseCallback: Receives the response to the request.
  typedef base::Callback<void(net::URLRequestContextGetter*,
                              const std::string&,
                              scoped_ptr<google::protobuf::MessageLite>,
                              const PostCleanupCallback&)> PostCallback;

  friend class RpcHandlerTest;

  void RegisterResponseHandler(const SuccessCallback& init_done_callback,
                               HttpPost* completed_post,
                               int http_status_code,
                               const std::string& response_data);
  void ReportResponseHandler(const StatusCallback& status_callback,
                             HttpPost* completed_post,
                             int http_status_code,
                             const std::string& response_data);

  void DispatchMessages(
      const google::protobuf::RepeatedPtrField<SubscribedMessage>&
      subscribed_messages);

  RequestHeader* CreateRequestHeader(const std::string& client_name) const;

  template <class T>
  void SendServerRequest(const std::string& rpc_name,
                         const std::string& app_id,
                         scoped_ptr<T> request,
                         const PostCleanupCallback& response_handler);

  // Wrapper for the http post constructor. This is the default way
  // to contact the server, but it can be overridden for testing.
  void SendHttpPost(net::URLRequestContextGetter* url_context_getter,
                    const std::string& rpc_name,
                    scoped_ptr<google::protobuf::MessageLite> request_proto,
                    const PostCleanupCallback& callback);

  // This method receives the request to encode a token and forwards it to
  // whispernet, setting the samples return callback to samples_callback.
  void AudioDirectiveListToWhispernetConnector(
      const std::string& token,
      bool audible,
      const WhispernetClient::SamplesCallback& samples_callback);

  CopresenceClientDelegate* delegate_;  // Belongs to the caller.
  TimedMap<std::string, bool> invalid_audio_token_cache_;
  PostCallback server_post_callback_;

  std::string device_id_;
  scoped_ptr<DirectiveHandler> directive_handler_;
  std::set<HttpPost*> pending_posts_;

  DISALLOW_COPY_AND_ASSIGN(RpcHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_
