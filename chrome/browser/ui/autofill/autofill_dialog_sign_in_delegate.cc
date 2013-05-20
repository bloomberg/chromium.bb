// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_sign_in_delegate.h"

#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

AutofillDialogSignInDelegate::AutofillDialogSignInDelegate(
    AutofillDialogView* dialog_view,
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      dialog_view_(dialog_view) {
  web_contents->SetDelegate(this);
}

void AutofillDialogSignInDelegate::ResizeDueToAutoResize(
    content::WebContents* source, const gfx::Size& pref_size) {
  dialog_view_->OnSignInResize(pref_size);
}

void AutofillDialogSignInDelegate::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  // Sizes to be used for the sign-in Window.
  const gfx::Size kSignInWindowMinSize(400, 331);
  const gfx::Size kSignInWindowMaxSize(800, 600);

  render_view_host->EnableAutoResize(kSignInWindowMinSize,
                                     kSignInWindowMaxSize);
  // Set the initial size as soon as we have an RVH to avoid
  // bad size jumping.
  dialog_view_->OnSignInResize(kSignInWindowMinSize);
}

}  // namespace autofill
