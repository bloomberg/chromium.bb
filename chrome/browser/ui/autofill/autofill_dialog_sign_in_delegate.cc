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
      dialog_view_(dialog_view),
      min_width_(400) {
  web_contents->SetDelegate(this);
}

void AutofillDialogSignInDelegate::ResizeDueToAutoResize(
    content::WebContents* source, const gfx::Size& pref_size) {
  dialog_view_->OnSignInResize(pref_size);
}

void AutofillDialogSignInDelegate::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  SetMinWidth(min_width_);

  // Set the initial size as soon as we have an RVH to avoid
  // bad size jumping.
  dialog_view_->OnSignInResize(GetMinSize());
}

void AutofillDialogSignInDelegate::SetMinWidth(int width) {
  min_width_ = width;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  if (host)
    host->EnableAutoResize(GetMinSize(), GetMaxSize());
}

gfx::Size AutofillDialogSignInDelegate::GetMinSize() const {
  return gfx::Size(min_width_, 331);
}

gfx::Size AutofillDialogSignInDelegate::GetMaxSize() const {
  return gfx::Size(std::max(min_width_, 800), 600);
}

}  // namespace autofill
