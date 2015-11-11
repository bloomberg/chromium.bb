// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_dialog_controller.h"

#include "chrome/browser/media/router/media_router_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

#if defined(OS_ANDROID)
#include "chrome/browser/media/android/router/media_router_dialog_controller_android.h"
#else
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_impl.h"
#endif

namespace media_router {

// static
MediaRouterDialogController*
MediaRouterDialogController::GetOrCreateForWebContents(
    content::WebContents* contents) {
#if defined(OS_ANDROID)
  return MediaRouterDialogControllerAndroid::GetOrCreateForWebContents(
      contents);
#else
  return MediaRouterDialogControllerImpl::GetOrCreateForWebContents(contents);
#endif
}

class MediaRouterDialogController::InitiatorWebContentsObserver
    : public content::WebContentsObserver {
 public:
  InitiatorWebContentsObserver(
      content::WebContents* web_contents,
      MediaRouterDialogController* dialog_controller)
      : content::WebContentsObserver(web_contents),
        dialog_controller_(dialog_controller) {
    DCHECK(dialog_controller_);
  }

 private:
  void WebContentsDestroyed() override {
    // NOTE: |this| is deleted after CloseMediaRouterDialog() returns.
    dialog_controller_->CloseMediaRouterDialog();
  }

  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override {
    // NOTE: |this| is deleted after CloseMediaRouterDialog() returns.
    dialog_controller_->CloseMediaRouterDialog();
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    // NOTE: |this| is deleted after CloseMediaRouterDialog() returns.
    dialog_controller_->CloseMediaRouterDialog();
  }

  MediaRouterDialogController* const dialog_controller_;
};

MediaRouterDialogController::MediaRouterDialogController(
    content::WebContents* initiator)
    : initiator_(initiator) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initiator_);
  initiator_observer_.reset(new InitiatorWebContentsObserver(initiator_, this));
}

MediaRouterDialogController::~MediaRouterDialogController() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool MediaRouterDialogController::ShowMediaRouterDialogForPresentation(
    scoped_ptr<CreatePresentationConnectionRequest> request) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Check if the media router dialog exists for |initiator| and return if so.
  if (IsShowingMediaRouterDialog())
    return false;

  create_connection_request_ = request.Pass();
  CreateMediaRouterDialog();

  // Show the initiator holding the existing media router dialog.
  ActivateInitiatorWebContents();

  media_router::MediaRouterMetrics::RecordMediaRouterDialogOrigin(
      media_router::PAGE);

  return true;
}

bool MediaRouterDialogController::ShowMediaRouterDialog() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Don't create dialog if it already exists.
  bool dialog_needs_creation = !IsShowingMediaRouterDialog();
  if (dialog_needs_creation)
    CreateMediaRouterDialog();

  ActivateInitiatorWebContents();
  return dialog_needs_creation;
}

void MediaRouterDialogController::HideMediaRouterDialog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CloseMediaRouterDialog();
  Reset();
}

void MediaRouterDialogController::ActivateInitiatorWebContents() {
  initiator_->GetDelegate()->ActivateContents(initiator_);
}

scoped_ptr<CreatePresentationConnectionRequest>
MediaRouterDialogController::TakeCreateConnectionRequest() {
  return create_connection_request_.Pass();
}

void MediaRouterDialogController::Reset() {
  initiator_observer_.reset();
  create_connection_request_.reset();
}

}  // namespace media_router
