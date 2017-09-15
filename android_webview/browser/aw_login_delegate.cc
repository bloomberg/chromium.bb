// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_login_delegate.h"

#include "android_webview/browser/aw_browser_context.h"
#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"
#include "net/url_request/url_request.h"

using namespace base::android;

using content::BrowserThread;
using content::RenderFrameHost;
using content::ResourceDispatcherHost;
using content::ResourceRequestInfo;
using content::WebContents;

namespace {
const char* kAuthAttemptsKey = "android_webview_auth_attempts";

class UrlRequestAuthAttemptsData : public base::SupportsUserData::Data {
 public:
  UrlRequestAuthAttemptsData() : auth_attempts_(0) { }
  int auth_attempts_;
};

}  // namespace

namespace android_webview {

AwLoginDelegate::AwLoginDelegate(net::AuthChallengeInfo* auth_info,
                                 net::URLRequest* request)
    : auth_info_(auth_info), request_(request) {
  UrlRequestAuthAttemptsData* count = static_cast<UrlRequestAuthAttemptsData*>(
      request->GetUserData(kAuthAttemptsKey));

  if (count == NULL) {
    count = new UrlRequestAuthAttemptsData();
    request->SetUserData(kAuthAttemptsKey, base::WrapUnique(count));
  }

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&AwLoginDelegate::HandleHttpAuthRequestOnUIThread, this,
                 (count->auth_attempts_ == 0),
                 request_info->GetWebContentsGetterForRequest()));
  count->auth_attempts_++;
}

AwLoginDelegate::~AwLoginDelegate() {
  // The Auth handler holds a ref count back on |this| object, so it should be
  // impossible to reach here while this object still owns an auth handler.
  DCHECK(!aw_http_auth_handler_);
}

void AwLoginDelegate::Proceed(const base::string16& user,
                              const base::string16& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&AwLoginDelegate::ProceedOnIOThread,
                 this, user, password));
}

void AwLoginDelegate::Cancel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&AwLoginDelegate::CancelOnIOThread, this));
}

void AwLoginDelegate::HandleHttpAuthRequestOnUIThread(
    bool first_auth_attempt,
    const content::ResourceRequestInfo::WebContentsGetter&
        web_contents_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = web_contents_getter.Run();
  aw_http_auth_handler_.reset(
      new AwHttpAuthHandler(this, auth_info_.get(), first_auth_attempt));
  if (!aw_http_auth_handler_->HandleOnUIThread(web_contents)) {
    Cancel();
    return;
  }
}

void AwLoginDelegate::CancelOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (request_) {
    request_->CancelAuth();
    ResourceDispatcherHost::Get()->ClearLoginDelegateForRequest(request_);
    request_ = NULL;
  }
  DeleteAuthHandlerSoon();
}

void AwLoginDelegate::ProceedOnIOThread(const base::string16& user,
                                        const base::string16& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (request_) {
    request_->SetAuth(net::AuthCredentials(user, password));
    ResourceDispatcherHost::Get()->ClearLoginDelegateForRequest(request_);
    request_ = NULL;
  }
  DeleteAuthHandlerSoon();
}

void AwLoginDelegate::OnRequestCancelled() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  request_ = NULL;
  DeleteAuthHandlerSoon();
}

void AwLoginDelegate::DeleteAuthHandlerSoon() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&AwLoginDelegate::DeleteAuthHandlerSoon, this));
    return;
  }
  aw_http_auth_handler_.reset();
}

}  // namespace android_webview
