// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/overlay_java_script_dialog_presenter.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_overlay.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#include "ios/chrome/browser/ui/dialogs/java_script_dialog_metrics.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using java_script_dialog_overlays::JavaScriptDialogRequest;
using java_script_dialog_overlays::JavaScriptDialogResponse;

namespace {
// Completion callback for JavaScript dialog overlays.
void HandleJavaScriptDialogResponse(web::DialogClosedCallback callback,
                                    web::WebState::Getter web_state_getter,
                                    OverlayResponse* response) {
  // Notify the blocking state that the dialog was shown.
  web::WebState* web_state = web_state_getter.Run();
  JavaScriptDialogBlockingState* blocking_state =
      web_state ? JavaScriptDialogBlockingState::FromWebState(web_state)
                : nullptr;
  if (blocking_state)
    blocking_state->JavaScriptDialogWasShown();

  JavaScriptDialogResponse* dialog_response =
      response ? response->GetInfo<JavaScriptDialogResponse>() : nullptr;
  if (!dialog_response) {
    // A null response is used if the dialog was not closed by user interaction.
    // This occurs either for navigation or because of WebState closures.
    IOSJavaScriptDialogDismissalCause cause =
        web_state ? IOSJavaScriptDialogDismissalCause::kNavigation
                  : IOSJavaScriptDialogDismissalCause::kClosure;
    RecordDialogDismissalCause(cause);
    std::move(callback).Run(/*success=*/false, /*user_input=*/nil);
    return;
  }

  // Update the blocking state if the suppression action was selected.
  JavaScriptDialogResponse::Action action = dialog_response->action();
  if (blocking_state &&
      action == JavaScriptDialogResponse::Action::kBlockDialogs) {
    blocking_state->JavaScriptDialogBlockingOptionSelected();
  }

  RecordDialogDismissalCause(IOSJavaScriptDialogDismissalCause::kUser);
  bool confirmed = action == JavaScriptDialogResponse::Action::kConfirm;
  NSString* user_input = confirmed ? dialog_response->user_input() : nil;
  std::move(callback).Run(confirmed, user_input);
}
}  // namespace

OverlayJavaScriptDialogPresenter::OverlayJavaScriptDialogPresenter() = default;

OverlayJavaScriptDialogPresenter::OverlayJavaScriptDialogPresenter(
    OverlayJavaScriptDialogPresenter&& other) = default;

OverlayJavaScriptDialogPresenter& OverlayJavaScriptDialogPresenter::operator=(
    OverlayJavaScriptDialogPresenter&& other) = default;

OverlayJavaScriptDialogPresenter::~OverlayJavaScriptDialogPresenter() = default;

void OverlayJavaScriptDialogPresenter::RunJavaScriptDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    web::JavaScriptDialogType dialog_type,
    NSString* message_text,
    NSString* default_prompt_text,
    web::DialogClosedCallback callback) {
  JavaScriptDialogBlockingState::CreateForWebState(web_state);
  if (JavaScriptDialogBlockingState::FromWebState(web_state)->blocked()) {
    // Block the dialog if needed.
    RecordDialogDismissalCause(IOSJavaScriptDialogDismissalCause::kBlocked);
    std::move(callback).Run(NO, nil);
    return;
  }

  bool from_main_frame_origin =
      origin_url.GetOrigin() == web_state->GetLastCommittedURL().GetOrigin();
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<JavaScriptDialogRequest>(
          dialog_type, web_state, origin_url, from_main_frame_origin,
          message_text, default_prompt_text);
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&HandleJavaScriptDialogResponse, std::move(callback),
                     web_state->CreateDefaultGetter()));
  OverlayRequestQueue::FromWebState(web_state, OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

void OverlayJavaScriptDialogPresenter::CancelDialogs(web::WebState* web_state) {
  OverlayRequestQueue::FromWebState(web_state, OverlayModality::kWebContentArea)
      ->CancelAllRequests();
}
