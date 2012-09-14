// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_LOGIN_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_LOGIN_DELEGATE_H_

#include "android_webview/browser/aw_http_auth_handler_base.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
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

  virtual void Proceed(const string16& user, const string16& password);
  virtual void Cancel();

  // from ResourceDispatcherHostLoginDelegate
  virtual void OnRequestCancelled() OVERRIDE;

 private:
  virtual ~AwLoginDelegate();
  void HandleHttpAuthRequestOnUIThread();
  void CancelOnIOThread();
  void ProceedOnIOThread(const string16& user, const string16& password);

  scoped_ptr<AwHttpAuthHandlerBase> aw_http_auth_handler_;
  scoped_refptr<net::AuthChallengeInfo> auth_info_;
  net::URLRequest* request_;
  int render_process_id_;
  int render_view_id_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_LOGIN_DELEGATE_H_
