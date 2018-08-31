// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_FRAME_IMPL_H_
#define IOS_WEB_WEB_STATE_WEB_FRAME_IMPL_H_

#include "ios/web/public/web_state/web_frame.h"

#include <map>
#include <string>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "crypto/symmetric_key.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "url/gurl.h"

namespace web {

class WebState;

class WebFrameImpl : public WebFrame, public web::WebStateObserver {
 public:
  // Creates a new WebFrame. |initial_message_id| will be used as the message ID
  // of the next message sent to the frame with the |CallJavaScriptFunction|
  // API.
  WebFrameImpl(const std::string& frame_id,
               std::unique_ptr<crypto::SymmetricKey> frame_key,
               int initial_message_id,
               bool is_main_frame,
               GURL security_origin,
               web::WebState* web_state);
  ~WebFrameImpl() override;

  // The associated web state.
  WebState* GetWebState();
  // Detaches the receiver from the associated  WebState.
  void DetachFromWebState();

  // WebFrame implementation
  std::string GetFrameId() const override;
  bool IsMainFrame() const override;
  GURL GetSecurityOrigin() const override;

  // WebStateObserver implementation
  void WebStateDestroyed(web::WebState* web_state) override;

  bool CallJavaScriptFunction(
      const std::string& name,
      const std::vector<base::Value>& parameters) override;
  bool CallJavaScriptFunction(
      std::string name,
      const std::vector<base::Value>& parameters,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) override;

 private:
  // Calls the JavaScript function |name| in the frame context in the same
  // manner as the inherited CallJavaScriptFunction functions. If
  // |reply_with_result| is true, the return value of executing the function
  // will be sent back to the receiver and handled by |OnJavaScriptReply|.
  bool CallJavaScriptFunction(const std::string& name,
                              const std::vector<base::Value>& parameters,
                              bool reply_with_result);

  // A structure to store the callbacks associated with the
  // |CallJavaScriptFunction| requests.
  typedef base::CancelableOnceCallback<void(void)> TimeoutCallback;
  struct RequestCallbacks {
    RequestCallbacks(base::OnceCallback<void(const base::Value*)> completion,
                     std::unique_ptr<TimeoutCallback>);
    ~RequestCallbacks();
    base::OnceCallback<void(const base::Value*)> completion;
    std::unique_ptr<TimeoutCallback> timeout_callback;
  };

  // Cancels the request associated with the message with id |message_id|. The
  // completion callback, if any, associated with |message_id| will be called
  // with a null result value. Note that the JavaScript will still run to
  // completion, but any future response will be ignored.
  void CancelRequest(int message_id);
  // Performs |CancelRequest| on all outstanding request callbacks in
  // |pending_requests_|.
  void CancelPendingRequests();
  // Calls the completion block of |request_callbacks| with a null value to
  // represent the request was cancelled.
  void CancelRequestWithCallbacks(
      std::unique_ptr<RequestCallbacks> request_callbacks);

  // Handles message from JavaScript with result of executing the function
  // specified in CallJavaScriptFunction.
  bool OnJavaScriptReply(web::WebState* web_state,
                         const base::DictionaryValue& command,
                         const GURL& page_url,
                         bool interacting,
                         bool is_main_frame);

  // The JavaScript requests awating a reply.
  std::map<uint32_t, std::unique_ptr<struct RequestCallbacks>>
      pending_requests_;

  // The frame identifier which uniquely identifies this frame across the
  // application's lifetime.
  std::string frame_id_;
  // The symmetric encryption key used to encrypt messages addressed to the
  // frame. Stored in a base64 encoded string.
  std::unique_ptr<crypto::SymmetricKey> frame_key_;
  // The message ID of the next JavaScript message to be sent.
  int next_message_id_ = 0;
  // Whether or not the receiver represents the main frame.
  bool is_main_frame_ = false;
  // The security origin associated with this frame.
  GURL security_origin_;
  // The associated web state.
  web::WebState* web_state_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebFrameImpl);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_FRAME_IMPL_H_
