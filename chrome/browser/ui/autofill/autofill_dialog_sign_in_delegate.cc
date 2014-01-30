// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_sign_in_delegate.h"

#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/window_open_disposition.h"

namespace autofill {

namespace {

using content::OpenURLParams;

// Signals if |params| require opening inside the current WebContents.
bool IsInPageTransition(const OpenURLParams& params) {
  return params.disposition == CURRENT_TAB;
}

// Indicates if the open action specified by |params| should happen in the
// Autofill dialog (when true) or in the browser (when false).
bool ShouldOpenInBrowser(const OpenURLParams& params) {
  return !IsInPageTransition(params) || !wallet::IsSignInRelatedUrl(params.url);
}

// Adjusts |params| to account for the fact that the open request originated in
// the dialog, but will be executed in the browser.
OpenURLParams AdjustToOpenInBrowser(const OpenURLParams& params) {
  if (!IsInPageTransition(params) || wallet::IsSignInRelatedUrl(params.url))
    return params;

  content::OpenURLParams new_params = params;
  new_params.disposition = NEW_FOREGROUND_TAB;
  return new_params;
}

}  // namespace

AutofillDialogSignInDelegate::AutofillDialogSignInDelegate(
    AutofillDialogView* dialog_view,
    content::WebContents* dialog_web_contents,
    content::WebContents* originating_web_contents,
    const gfx::Size& minimum_size,
    const gfx::Size& maximum_size)
    : WebContentsObserver(dialog_web_contents),
      dialog_view_(dialog_view),
      originating_web_contents_(originating_web_contents) {
  DCHECK(dialog_view_);
  dialog_web_contents->SetDelegate(this);

  content::RendererPreferences* prefs =
      dialog_web_contents->GetMutableRendererPrefs();
  prefs->browser_handles_non_local_top_level_requests = true;
  dialog_web_contents->GetRenderViewHost()->SyncRendererPrefs();

  UpdateLimitsAndEnableAutoResize(minimum_size, maximum_size);
}

void AutofillDialogSignInDelegate::ResizeDueToAutoResize(
    content::WebContents* source,
    const gfx::Size& preferred_size) {
  dialog_view_->OnSignInResize(preferred_size);
}

content::WebContents* AutofillDialogSignInDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (ShouldOpenInBrowser(params))
    return originating_web_contents_->OpenURL(AdjustToOpenInBrowser(params));

  source->GetController().LoadURL(params.url,
                                  params.referrer,
                                  params.transition,
                                  std::string());
  return source;
}

// This gets invoked whenever there is an attempt to open a new window/tab.
// Reroute to the original browser.
void AutofillDialogSignInDelegate::AddNewContents(
    content::WebContents* source,
    content::WebContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture,
    bool* was_blocked) {
  chrome::AddWebContents(
      chrome::FindBrowserWithWebContents(originating_web_contents_),
      source, new_contents, disposition, initial_pos, user_gesture,
      was_blocked);
}

bool AutofillDialogSignInDelegate::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return event.type == blink::WebGestureEvent::GesturePinchBegin ||
      event.type == blink::WebGestureEvent::GesturePinchUpdate ||
      event.type == blink::WebGestureEvent::GesturePinchEnd;
}

void AutofillDialogSignInDelegate::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  EnableAutoResize();

  // Set the initial size as soon as we have an RVH to avoid bad size jumping.
  dialog_view_->OnSignInResize(minimum_size_);
}

void AutofillDialogSignInDelegate::UpdateLimitsAndEnableAutoResize(
    const gfx::Size& minimum_size,
    const gfx::Size& maximum_size) {
  minimum_size_ = minimum_size;
  maximum_size_ = maximum_size;
  EnableAutoResize();
}

void AutofillDialogSignInDelegate::EnableAutoResize() {
  DCHECK(!minimum_size_.IsEmpty());
  DCHECK(!maximum_size_.IsEmpty());
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  if (host)
    host->EnableAutoResize(minimum_size_, maximum_size_);
}

}  // namespace autofill
