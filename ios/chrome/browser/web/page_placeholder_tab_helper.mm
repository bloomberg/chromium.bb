// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Placeholder will not be displayed longer than this time.
const double kPlaceholderMaxDisplayTimeInSeconds = 1.5;

// Placeholder removal will include a fade-out animation of this length.
const NSTimeInterval kPlaceholderFadeOutAnimationLengthInSeconds = 0.5;
}  // namespace

PagePlaceholderTabHelper::PagePlaceholderTabHelper(web::WebState* web_state)
    : web_state_(web_state), weak_factory_(this) {
  web_state_->AddObserver(this);
}

PagePlaceholderTabHelper::~PagePlaceholderTabHelper() {
  RemovePlaceholder();
}

void PagePlaceholderTabHelper::AddPlaceholderForNextNavigation() {
  add_placeholder_for_next_navigation_ = true;
}

void PagePlaceholderTabHelper::CancelPlaceholderForNextNavigation() {
  add_placeholder_for_next_navigation_ = false;
  if (displaying_placeholder_) {
    RemovePlaceholder();
  }
}

void PagePlaceholderTabHelper::WasShown(web::WebState* web_state) {
  if (add_placeholder_for_next_navigation_) {
    AddPlaceholder();
  }
}

void PagePlaceholderTabHelper::WasHidden(web::WebState* web_state) {
  RemovePlaceholder();
}

void PagePlaceholderTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  if (add_placeholder_for_next_navigation_) {
    add_placeholder_for_next_navigation_ = false;
    AddPlaceholder();
  }
}

void PagePlaceholderTabHelper::PageLoaded(web::WebState* web_state,
                                          web::PageLoadCompletionStatus) {
  DCHECK_EQ(web_state_, web_state);
  RemovePlaceholder();
}

void PagePlaceholderTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  RemovePlaceholder();
}

void PagePlaceholderTabHelper::AddPlaceholder() {
  if (displaying_placeholder_)
    return;

  displaying_placeholder_ = true;

  // Lazily create the placeholder view.
  if (!placeholder_view_) {
    placeholder_view_ = [[TopAlignedImageView alloc] init];
    placeholder_view_.backgroundColor = [UIColor whiteColor];
    placeholder_view_.translatesAutoresizingMaskIntoConstraints = NO;
  }

  // Update placeholder view's image and display it on top of WebState's view.
  SnapshotTabHelper* snapshotTabHelper =
      SnapshotTabHelper::FromWebState(web_state_);
  if (snapshotTabHelper) {
    base::WeakPtr<PagePlaceholderTabHelper> weak_tab_helper =
        weak_factory_.GetWeakPtr();
    snapshotTabHelper->RetrieveGreySnapshot(^(UIImage* snapshot) {
      if (weak_tab_helper && weak_tab_helper->displaying_placeholder()) {
        DisplaySnapshotImage(snapshot);
      }
    });
  }

  // Remove placeholder if it takes too long to load the page.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PagePlaceholderTabHelper::RemovePlaceholder,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSecondsD(kPlaceholderMaxDisplayTimeInSeconds));
}

void PagePlaceholderTabHelper::DisplaySnapshotImage(UIImage* snapshot) {
  placeholder_view_.image = snapshot;
  [web_state_->GetView() addSubview:placeholder_view_];
  AddSameConstraints([NamedGuide guideWithName:kContentAreaGuide
                                          view:placeholder_view_],
                     placeholder_view_);
}

void PagePlaceholderTabHelper::RemovePlaceholder() {
  if (!displaying_placeholder_)
    return;

  displaying_placeholder_ = false;

  // Remove placeholder view with a fade-out animation.
  __weak UIView* weak_placeholder_view = placeholder_view_;
  [UIView animateWithDuration:kPlaceholderFadeOutAnimationLengthInSeconds
      animations:^{
        weak_placeholder_view.alpha = 0.0f;
      }
      completion:^(BOOL finished) {
        [weak_placeholder_view removeFromSuperview];
        weak_placeholder_view.alpha = 1.0f;
      }];
}

WEB_STATE_USER_DATA_KEY_IMPL(PagePlaceholderTabHelper)
