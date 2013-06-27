// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_SIGN_IN_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_SIGN_IN_DELEGATE_H_

#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

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
                               content::WebContents* web_contents);

  // WebContentsDelegate implementation.
  virtual void ResizeDueToAutoResize(content::WebContents* source,
                                     const gfx::Size& pref_size) OVERRIDE;

  // WebContentsObserver implementation.
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // Sets the minimum width for the render view. This should be set to the
  // width of the host AutofillDialogView.
  void SetMinWidth(int width);

 private:
  // Gets the minimum and maximum size for the dialog.
  gfx::Size GetMinSize() const;
  gfx::Size GetMaxSize() const;

  AutofillDialogView* dialog_view_;
  int min_width_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_SIGN_IN_DELEGATE_H_
