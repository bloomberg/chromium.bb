// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_LOGIN_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_LOGIN_DELEGATE_H_

#include <memory>

#include "android_webview/browser/aw_http_auth_handler.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"

namespace net {
class AuthChallengeInfo;
class URLRequest;
}

namespace android_webview {

class AwLoginDelegate :
    public content::ResourceDispatcherHostLoginDelegate {
 public:
  AwLoginDelegate(net::AuthChallengeInfo* auth_info,
                  net::URLRequest* request);

  virtual void Proceed(const base::string16& user,
                       const base::string16& password);
  virtual void Cancel();

  // from ResourceDispatcherHostLoginDelegate
  void OnRequestCancelled() override;

 private:
  ~AwLoginDelegate() override;
  void HandleHttpAuthRequestOnUIThread(bool first_auth_attempt);
  void CancelOnIOThread();
  void ProceedOnIOThread(const base::string16& user,
                         const base::string16& password);
  void DeleteAuthHandlerSoon();

  std::unique_ptr<AwHttpAuthHandler> aw_http_auth_handler_;
  scoped_refptr<net::AuthChallengeInfo> auth_info_;
  net::URLRequest* request_;
  int render_process_id_;
  int render_frame_id_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_LOGIN_DELEGATE_H_
