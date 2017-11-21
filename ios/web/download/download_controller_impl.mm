// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_controller_impl.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/download/download_controller_delegate.h"
#include "ios/web/public/web_thread.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kDownloadControllerKey = 0;
}  // namespace

namespace web {

// static
DownloadController* DownloadController::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);
  if (!browser_state->GetUserData(&kDownloadControllerKey)) {
    browser_state->SetUserData(&kDownloadControllerKey,
                               std::make_unique<DownloadControllerImpl>());
  }
  return static_cast<DownloadControllerImpl*>(
      browser_state->GetUserData(&kDownloadControllerKey));
}

DownloadControllerImpl::DownloadControllerImpl() = default;

DownloadControllerImpl::~DownloadControllerImpl() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  for (DownloadTaskImpl* task : alive_tasks_)
    task->ShutDown();

  if (delegate_)
    delegate_->OnDownloadControllerDestroyed(this);

  DCHECK(!delegate_);
}

void DownloadControllerImpl::CreateDownloadTask(
    const WebState* web_state,
    NSString* identifier,
    const GURL& original_url,
    const std::string& content_disposition,
    int64_t total_bytes,
    const std::string& mime_type) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (!delegate_)
    return;

  auto task = std::make_unique<DownloadTaskImpl>(
      web_state, original_url, content_disposition, total_bytes, mime_type,
      identifier, this);
  alive_tasks_.insert(task.get());
  delegate_->OnDownloadCreated(this, web_state, std::move(task));
}

void DownloadControllerImpl::SetDelegate(DownloadControllerDelegate* delegate) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  delegate_ = delegate;
}

void DownloadControllerImpl::OnTaskDestroyed(DownloadTaskImpl* task) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  auto it = alive_tasks_.find(task);
  DCHECK(it != alive_tasks_.end());
  alive_tasks_.erase(it);
}

NSURLSession* DownloadControllerImpl::CreateSession(
    NSString* identifier,
    id<NSURLSessionDataDelegate> delegate,
    NSOperationQueue* delegate_queue) {
  NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration
      backgroundSessionConfigurationWithIdentifier:identifier];
  return [NSURLSession sessionWithConfiguration:configuration
                                       delegate:delegate
                                  delegateQueue:delegate_queue];
}

}  // namespace web
