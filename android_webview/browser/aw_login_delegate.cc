// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_login_delegate.h"

#include "base/logging.h"
#include "base/android/jni_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"
#include "net/url_request/url_request.h"

using namespace base::android;

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceDispatcherHost;
using content::ResourceRequestInfo;
using content::WebContents;

namespace android_webview {

AwLoginDelegate::AwLoginDelegate(net::AuthChallengeInfo* auth_info,
                                 net::URLRequest* request)
    : auth_info_(auth_info),
      request_(request),
      render_process_id_(0),
      render_view_id_(0) {
    // TODO(benm): We need to track whether the last auth request for this host
    // was successful (for HttpAuthHandler.useHttpAuthUsernamePassword()). We
    // could attach that to the URLRequests SupportsUserData::Data and bubble
    // that up to Java for the purpose of the API.
    ResourceRequestInfo::GetRenderViewForRequest(
        request, &render_process_id_, &render_view_id_);

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&AwLoginDelegate::HandleHttpAuthRequestOnUIThread,
                   make_scoped_refptr(this)));
}

AwLoginDelegate::~AwLoginDelegate() {
}

void AwLoginDelegate::Proceed(const string16& user,
                              const string16& password) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&AwLoginDelegate::ProceedOnIOThread,
                 make_scoped_refptr(this), user, password));
}

void AwLoginDelegate::Cancel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&AwLoginDelegate::CancelOnIOThread,
                 make_scoped_refptr(this)));
}

void AwLoginDelegate::HandleHttpAuthRequestOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  aw_http_auth_handler_.reset(AwHttpAuthHandlerBase::Create(this, auth_info_));

  RenderViewHost* render_view_host = RenderViewHost::FromID(
      render_process_id_, render_view_id_);
  if (!render_view_host) {
    Cancel();
    return;
  }

  WebContents* web_contents = WebContents::FromRenderViewHost(
      render_view_host);
  aw_http_auth_handler_->HandleOnUIThread(web_contents);
}

void AwLoginDelegate::CancelOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (request_) {
    request_->CancelAuth();
    ResourceDispatcherHost::Get()->ClearLoginDelegateForRequest(request_);
  }
}

void AwLoginDelegate::ProceedOnIOThread(const string16& user,
                                        const string16& password) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (request_) {
    request_->SetAuth(net::AuthCredentials(user, password));
    ResourceDispatcherHost::Get()->ClearLoginDelegateForRequest(request_);
  }
}

void AwLoginDelegate::OnRequestCancelled() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  request_ = NULL;
  aw_http_auth_handler_.reset();
}

}  // namespace android_webview
