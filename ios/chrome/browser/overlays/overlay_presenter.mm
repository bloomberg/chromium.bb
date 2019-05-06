// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_presenter.h"

#include "base/logging.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OverlayPresenter::OverlayPresenter(OverlayModality modality,
                                   WebStateList* web_state_list)
    : modality_(modality),
      web_state_list_(web_state_list),
      weak_factory_(this) {
  DCHECK(web_state_list_);
  web_state_list_->AddObserver(this);
  for (int i = 0; i < web_state_list_->count(); ++i) {
    GetQueueForWebState(web_state_list_->GetWebStateAt(i))->AddObserver(this);
  }
  SetActiveWebState(web_state_list_->GetActiveWebState(), CHANGE_REASON_NONE);
}

OverlayPresenter::~OverlayPresenter() {
  // Disconnect() must be called before destruction.
  DCHECK(!ui_delegate_);
  DCHECK(!web_state_list_);
}

#pragma mark - Public

void OverlayPresenter::SetUIDelegate(OverlayUIDelegate* ui_delegate) {
  // When the UI delegate is reset, the presenter will begin showing overlays in
  // the new delegate's presentation context.  Cancel overlay state from the
  // previous delegate since this Browser's overlays will no longer be presented
  // there.
  if (ui_delegate_)
    CancelAllOverlayUI();

  ui_delegate_ = ui_delegate;

  // Reset |presenting| since it was tracking the status for the previous
  // delegate's presentation context.
  presenting_ = false;
  if (ui_delegate_)
    PresentOverlayForActiveRequest();
}

void OverlayPresenter::Disconnect() {
  SetUIDelegate(nullptr);
  SetActiveWebState(nullptr, CHANGE_REASON_NONE);

  for (int i = 0; i < web_state_list_->count(); ++i) {
    GetQueueForWebState(web_state_list_->GetWebStateAt(i))
        ->RemoveObserver(this);
  }
  web_state_list_->RemoveObserver(this);
  web_state_list_ = nullptr;
}

#pragma mark -
#pragma mark Accessors

void OverlayPresenter::SetActiveWebState(
    web::WebState* web_state,
    WebStateListObserver::ChangeReason reason) {
  if (active_web_state_ == web_state)
    return;

  web::WebState* previously_active_web_state = active_web_state_;

  // The UI should be cancelled instead of hidden if the presenter does not
  // expect to show any more overlay UI for previously active WebState in the UI
  // delegate's presentation context.  This occurs:
  // - when the active WebState is replaced, and
  // - when the active WebState is detached from the WebStateList.
  bool should_cancel_ui =
      (reason & CHANGE_REASON_REPLACED) || detaching_active_web_state_;

  active_web_state_ = web_state;
  detaching_active_web_state_ = false;

  // Early return if there's no UI delegate, since presentation cannot occur.
  if (!ui_delegate_)
    return;

  // If not already presenting, immediately show the next overlay.
  if (!presenting_) {
    PresentOverlayForActiveRequest();
    return;
  }

  // If the active WebState changes while an overlay is being presented, the
  // presented UI needs to be dismissed before the next overlay for the new
  // active WebState can be shown.  The new active WebState's overlays will be
  // presented when the previous overlay's dismissal callback is executed.
  DCHECK(previously_active_web_state);
  if (should_cancel_ui) {
    CancelOverlayUIForWebState(previously_active_web_state);
  } else {
    // For WebState activations, the overlay UI for the previously active
    // WebState should be hidden, as it may be shown again upon reactivating.
    ui_delegate_->HideOverlayUIForWebState(previously_active_web_state,
                                           modality_);
  }
}

OverlayRequestQueueImpl* OverlayPresenter::GetQueueForWebState(
    web::WebState* web_state) const {
  if (!web_state)
    return nullptr;
  OverlayRequestQueueImpl::Container::CreateForWebState(web_state);
  return OverlayRequestQueueImpl::Container::FromWebState(web_state)
      ->QueueForModality(modality_);
}

OverlayRequestQueueImpl* OverlayPresenter::GetActiveQueue() const {
  return GetQueueForWebState(active_web_state_);
}

OverlayRequest* OverlayPresenter::GetActiveRequest() const {
  OverlayRequestQueueImpl* queue = GetActiveQueue();
  return queue ? queue->front_request() : nullptr;
}

#pragma mark UI Presentation and Dismissal helpers

void OverlayPresenter::PresentOverlayForActiveRequest() {
  // Overlays cannot be presented if one is already presented.
  DCHECK(!presenting_);

  // Overlays cannot be shown without a UI delegate.
  if (!ui_delegate_)
    return;

  // No presentation is necessary if there is no active reqeust.
  OverlayRequest* request = GetActiveRequest();
  if (!request)
    return;

  presenting_ = true;

  OverlayDismissalCallback dismissal_callback = base::BindOnce(
      &OverlayPresenter::OverlayWasDismissed, weak_factory_.GetWeakPtr(),
      ui_delegate_, request, GetActiveQueue());
  ui_delegate_->ShowOverlayUIForWebState(active_web_state_, modality_,
                                         std::move(dismissal_callback));
}

void OverlayPresenter::OverlayWasDismissed(OverlayUIDelegate* ui_delegate,
                                           OverlayRequest* request,
                                           OverlayRequestQueueImpl* queue,
                                           OverlayDismissalReason reason) {
  // If the UI delegate is reset while presenting an overlay, that overlay will
  // be cancelled and dismissed.  The presenter is now using the new UI
  // delegate's presentation context, so this dismissal should not trigger
  // presentation logic.
  if (ui_delegate_ != ui_delegate)
    return;

  // If the overlay was dismissed for user interaction, pop it from the queue
  // since it will never be shown again.
  // TODO(crbug.com/941745): Prevent the queue from being accessed if deleted.
  // This is possible if a WebState is closed during an overlay dismissal
  // animation triggered by user interaction.
  if (reason == OverlayDismissalReason::kUserInteraction)
    queue->PopFrontRequest();

  presenting_ = false;

  // Only show the next overlay if the active request has changed, either
  // because the frontmost request was popped or because the active WebState has
  // changed.
  if (GetActiveRequest() != request)
    PresentOverlayForActiveRequest();
}

#pragma mark Cancellation helpers

void OverlayPresenter::CancelOverlayUIForWebState(web::WebState* web_state) {
  if (ui_delegate_)
    ui_delegate_->CancelOverlayUIForWebState(web_state, modality_);
}

void OverlayPresenter::CancelAllOverlayUI() {
  for (int i = 0; i < web_state_list_->count(); ++i) {
    CancelOverlayUIForWebState(web_state_list_->GetWebStateAt(i));
  }
}

#pragma mark - OverlayRequestQueueImpl::Observer

void OverlayPresenter::RequestAddedToQueue(OverlayRequestQueueImpl* queue,
                                           OverlayRequest* request) {
  // If |queue| is active and the added request is the front request, trigger
  // the UI presentation for that request.
  if (queue == GetActiveQueue() && request == queue->front_request())
    PresentOverlayForActiveRequest();
}

#pragma mark - WebStateListObserver

void OverlayPresenter::WebStateInsertedAt(WebStateList* web_state_list,
                                          web::WebState* web_state,
                                          int index,
                                          bool activating) {
  GetQueueForWebState(web_state)->AddObserver(this);
}

void OverlayPresenter::WebStateReplacedAt(WebStateList* web_state_list,
                                          web::WebState* old_web_state,
                                          web::WebState* new_web_state,
                                          int index) {
  GetQueueForWebState(old_web_state)->RemoveObserver(this);
  GetQueueForWebState(new_web_state)->AddObserver(this);
  if (old_web_state != active_web_state_) {
    // If the active WebState is being replaced, its overlay UI will be
    // cancelled later when |new_web_state| is activated.  For inactive WebState
    // replacements, the overlay UI can be cancelled immediately.
    CancelOverlayUIForWebState(old_web_state);
  }
}

void OverlayPresenter::WillDetachWebStateAt(WebStateList* web_state_list,
                                            web::WebState* web_state,
                                            int index) {
  GetQueueForWebState(web_state)->RemoveObserver(this);
  detaching_active_web_state_ = web_state == active_web_state_;
  if (!detaching_active_web_state_) {
    // If the active WebState is being detached, its overlay UI will be
    // cancelled later when the active WebState is reset.  For inactive WebState
    // replacements, the overlay UI can be cancelled immediately.
    CancelOverlayUIForWebState(web_state);
  }
}

void OverlayPresenter::WebStateActivatedAt(WebStateList* web_state_list,
                                           web::WebState* old_web_state,
                                           web::WebState* new_web_state,
                                           int active_index,
                                           int reason) {
  SetActiveWebState(new_web_state,
                    static_cast<WebStateListObserver::ChangeReason>(reason));
}
