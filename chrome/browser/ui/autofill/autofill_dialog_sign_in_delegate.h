// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_SIGN_IN_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_SIGN_IN_DELEGATE_H_

#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/size.h"

namespace autofill {

class AutofillDialogView;

// AutofillDialogSignInDelegate provides a WebContentsDelegate and
// WebContentsObserver for the sign-in page in the autofill dialog. Allows the
// dialog to resize based on the size of the hosted web page.
// TODO(abodenha) It probably makes sense to move the NotificationObserver
// for detecting completed sign-in to this class instead of
// AutofillDialogControllerImpl.
class AutofillDialogSignInDelegate: public content::WebContentsDelegate,
                                    public content::WebContentsObserver {
 public:
  AutofillDialogSignInDelegate(AutofillDialogView* dialog_view,
                               content::WebContents* web_contents,
                               content::WebContentsDelegate* wrapped_delegate,
                               const gfx::Size& minimum_size,
                               const gfx::Size& maximum_size);

  // WebContentsDelegate implementation.
  virtual void ResizeDueToAutoResize(content::WebContents* source,
                                     const gfx::Size& preferred_size) OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;

  // WebContentsObserver implementation.
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;

 private:
  // Enables auto-resizing for this view, if possible, constrained to the
  // minimum and maximum size allowed by the delegate.
  void EnableAutoResize();

  // The dialog view hosting this sign in page.
  AutofillDialogView* const dialog_view_;

  // The delegate for the WebContents hosting this dialog.
  content::WebContentsDelegate* const wrapped_delegate_;

  // The minimum and maximum sizes that the sign-in view may have.
  const gfx::Size minimum_size_;
  const gfx::Size maximum_size_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_SIGN_IN_DELEGATE_H_
