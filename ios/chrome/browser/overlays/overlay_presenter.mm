// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_presenter.h"

#include "base/logging.h"
#import "ios/chrome/browser/overlays/overlay_request_queue_impl.h"
#import "ios/chrome/browser/overlays/public/overlay_ui_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OverlayPresenter::OverlayPresenter(OverlayModality modality,
                                   WebStateList* web_state_list)
    : modality_(modality), web_state_list_(web_state_list) {
  DCHECK(web_state_list_);
}

OverlayPresenter::~OverlayPresenter() {
  // Disconnect() must be called before destruction.
  DCHECK(!ui_delegate_);
  DCHECK(!web_state_list_);
}

#pragma mark - Public

void OverlayPresenter::SetUIDelegate(OverlayUIDelegate* ui_delegate) {
  if (ui_delegate_) {
    // TODO:(crbug.com/941745): Cancel all overlays for previous UI delegate.
    StopObserving();
  }
  ui_delegate_ = ui_delegate;
  if (ui_delegate_) {
    StartObserving();
    // TODO:(crbug.com/941745): Show overlay for frontmost request in active
    // WebState's queue.
  }
}

void OverlayPresenter::Disconnect() {
  SetUIDelegate(nullptr);
  web_state_list_ = nullptr;
}

#pragma mark - Private

OverlayRequestQueueImpl* OverlayPresenter::GetQueueForWebState(
    web::WebState* web_state) {
  OverlayRequestQueueImpl::Container::CreateForWebState(web_state);
  return OverlayRequestQueueImpl::Container::FromWebState(web_state)
      ->QueueForModality(modality_);
}

void OverlayPresenter::StartObserving() {
  web_state_list_->AddObserver(this);
  for (int i = 0; i < web_state_list_->count(); ++i) {
    GetQueueForWebState(web_state_list_->GetWebStateAt(i))->AddObserver(this);
  }
}

void OverlayPresenter::StopObserving() {
  for (int i = 0; i < web_state_list_->count(); ++i) {
    GetQueueForWebState(web_state_list_->GetWebStateAt(i))
        ->RemoveObserver(this);
  }
  web_state_list_->RemoveObserver(this);
}

#pragma mark - OverlayRequestQueueImplObserver

void OverlayPresenter::OnRequestAdded(OverlayRequestQueueImpl* queue,
                                      OverlayRequest* request) {
  // If the added request is first in the queue, trigger the UI presentation for
  // that reqeust.
  if (request == queue->front_request()) {
    // TODO:(crbug.com/941745): Trigger presentation for request.
  }
}

#pragma mark - WebStateListObserver

void OverlayPresenter::WebStateInsertedAt(WebStateList* web_state_list,
                                          web::WebState* web_state,
                                          int index,
                                          bool activating) {
  GetQueueForWebState(web_state)->AddObserver(this);
  if (activating) {
    // TODO:(crbug.com/941745): If showing an overlay, dismiss it.  Then show
    // the overlay for the frontmost request in |web_state|'s queue.
  }
}

void OverlayPresenter::WebStateReplacedAt(WebStateList* web_state_list,
                                          web::WebState* old_web_state,
                                          web::WebState* new_web_state,
                                          int index) {
  GetQueueForWebState(old_web_state)->RemoveObserver(this);
  GetQueueForWebState(new_web_state)->AddObserver(this);
  // TODO:(crbug.com/941745): Show overlay for first request in |new_web_state|
  // if it's active.
}

void OverlayPresenter::WillDetachWebStateAt(WebStateList* web_state_list,
                                            web::WebState* web_state,
                                            int index) {
  GetQueueForWebState(web_state)->RemoveObserver(this);
  // TODO:(crbug.com/941745): Cancel overlays for |web_state|.
}

void OverlayPresenter::WebStateActivatedAt(WebStateList* web_state_list,
                                           web::WebState* old_web_state,
                                           web::WebState* new_web_state,
                                           int active_index,
                                           int reason) {
  // TODO:(crbug.com/941745): If showing an overlay, dismiss it.  Then show the
  // overlay for the frontmost request in |web_state|'s queue.
}
