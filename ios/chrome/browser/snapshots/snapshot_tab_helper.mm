// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/snapshots/snapshot_generator.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SnapshotTabHelper::~SnapshotTabHelper() {
  DCHECK(!web_state_);
}

// static
void SnapshotTabHelper::CreateForWebState(web::WebState* web_state,
                                          NSString* session_id) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new SnapshotTabHelper(web_state, session_id)));
  }
}

void SnapshotTabHelper::SetDelegate(id<SnapshotGeneratorDelegate> delegate) {
  snapshot_generator_.delegate = delegate;
}

CGSize SnapshotTabHelper::GetSnapshotSize() const {
  return [snapshot_generator_ snapshotSize];
}

void SnapshotTabHelper::RetrieveColorSnapshot(void (^callback)(UIImage*)) {
  [snapshot_generator_ retrieveSnapshot:callback];
}

void SnapshotTabHelper::RetrieveGreySnapshot(void (^callback)(UIImage*)) {
  [snapshot_generator_ retrieveGreySnapshot:callback];
}

void SnapshotTabHelper::UpdateSnapshotWithCallback(void (^callback)(UIImage*)) {
  if (web_state_->ContentIsHTML()) {
    [snapshot_generator_ updateWebViewSnapshotWithCompletion:callback];
    return;
  }
  // Native content cannot utilize the WKWebView snapshotting API.
  UIImage* image = UpdateSnapshot();
  dispatch_async(dispatch_get_main_queue(), ^{
    if (callback)
      callback(image);
  });
}

UIImage* SnapshotTabHelper::UpdateSnapshot() {
  return [snapshot_generator_ updateSnapshot];
}

UIImage* SnapshotTabHelper::GenerateSnapshotWithoutOverlays() {
  return [snapshot_generator_ generateSnapshotWithOverlays:NO];
}

void SnapshotTabHelper::SetSnapshotCoalescingEnabled(bool enabled) {
  [snapshot_generator_ setSnapshotCoalescingEnabled:enabled];
}

void SnapshotTabHelper::RemoveSnapshot() {
  DCHECK(web_state_);
  [snapshot_generator_ removeSnapshot];
}

void SnapshotTabHelper::IgnoreNextLoad() {
  ignore_next_load_ = true;
}

// static
UIImage* SnapshotTabHelper::GetDefaultSnapshotImage() {
  return [SnapshotGenerator defaultSnapshotImage];
}

SnapshotTabHelper::SnapshotTabHelper(web::WebState* web_state,
                                     NSString* session_id)
    : web_state_(web_state),
      web_state_observer_(this),
      infobar_observer_(this),
      weak_ptr_factory_(this) {
  snapshot_generator_ = [[SnapshotGenerator alloc] initWithWebState:web_state_
                                                  snapshotSessionId:session_id];
  web_state_observer_.Add(web_state_);

  // Supports missing InfoBarManager to make testing easier.
  infobar_manager_ = InfoBarManagerImpl::FromWebState(web_state_);
  if (infobar_manager_) {
    infobar_observer_.Add(infobar_manager_);
  }
}

void SnapshotTabHelper::DidStartLoading(web::WebState* web_state) {
    RemoveSnapshot();
}

void SnapshotTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (!ignore_next_load_ &&
      load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    if (web_state->ContentIsHTML()) {
      base::PostDelayedTaskWithTraits(
          FROM_HERE, {web::WebThread::UI},
          base::BindOnce(&SnapshotTabHelper::UpdateSnapshotWithCallback,
                         weak_ptr_factory_.GetWeakPtr(), /*callback=*/nil),
          base::TimeDelta::FromSeconds(1));
      return;
    }
    // Native content cannot utilize the WKWebView snapshotting API.
    UpdateSnapshot();
  }
  ignore_next_load_ = false;
}

void SnapshotTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_observer_.Remove(web_state);
  web_state_ = nullptr;
}

void SnapshotTabHelper::OnInfoBarAdded(infobars::InfoBar* infobar) {
  UpdateSnapshotWithCallback(nil);
}

void SnapshotTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                         bool animate) {
  UpdateSnapshotWithCallback(nil);
}

void SnapshotTabHelper::OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                                          infobars::InfoBar* new_infobar) {
  UpdateSnapshotWithCallback(nil);
}

void SnapshotTabHelper::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  DCHECK_EQ(infobar_manager_, manager);
  infobar_observer_.Remove(manager);
  infobar_manager_ = nullptr;
}
