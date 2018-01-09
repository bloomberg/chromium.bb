// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/download_manager_tab_helper.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/download/download_manager_tab_helper_delegate.h"
#import "ios/web/public/download/download_task.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(DownloadManagerTabHelper);

DownloadManagerTabHelper::DownloadManagerTabHelper(
    web::WebState* web_state,
    id<DownloadManagerTabHelperDelegate> delegate)
    : web_state_(web_state), delegate_(delegate) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);
}

DownloadManagerTabHelper::~DownloadManagerTabHelper() {
  DCHECK(!task_);
}

void DownloadManagerTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<DownloadManagerTabHelperDelegate> delegate) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new DownloadManagerTabHelper(web_state, delegate)));
  }
}

void DownloadManagerTabHelper::Download(
    std::unique_ptr<web::DownloadTask> task) {
  task_ = std::move(task);
  task_->AddObserver(this);
  [delegate_ downloadManagerTabHelper:this didCreateDownload:task_.get()];
}

void DownloadManagerTabHelper::WasShown(web::WebState* web_state) {
  if (task_) {
    [delegate_ downloadManagerTabHelper:this didShowDownload:task_.get()];
  }
}

void DownloadManagerTabHelper::WasHidden(web::WebState* web_state) {
  if (task_) {
    [delegate_ downloadManagerTabHelper:this didHideDownload:task_.get()];
  }
}

void DownloadManagerTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  if (task_) {
    task_->RemoveObserver(this);
    task_ = nullptr;
  }
}

void DownloadManagerTabHelper::OnDownloadUpdated(web::DownloadTask* task) {
  DCHECK_EQ(task, task_.get());
  switch (task->GetState()) {
    case web::DownloadTask::State::kCancelled:
      task_->RemoveObserver(this);
      task_ = nullptr;
      break;
    case web::DownloadTask::State::kInProgress:
    case web::DownloadTask::State::kComplete:
      [delegate_ downloadManagerTabHelper:this didUpdateDownload:task_.get()];
      break;
    case web::DownloadTask::State::kNotStarted:
      // OnDownloadUpdated cannot be called with this state.
      NOTREACHED();
  }
}
