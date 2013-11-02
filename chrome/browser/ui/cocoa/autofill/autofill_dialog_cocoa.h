// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/testable_autofill_dialog_view.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "ui/gfx/size.h"

namespace content {
  class NavigationController;
}

namespace autofill {
  class AutofillDialogViewDelegate;
}

@class AutofillAccountChooser;
@class AutofillDialogWindowController;
@class AutofillMainContainer;
@class AutofillOverlayController;
@class AutofillSignInContainer;

namespace autofill {

class AutofillDialogCocoa : public AutofillDialogView,
                            public TestableAutofillDialogView,
                            public ConstrainedWindowMacDelegate {
 public:
  explicit AutofillDialogCocoa(AutofillDialogViewDelegate* delegate);
  virtual ~AutofillDialogCocoa();

  // AutofillDialogView implementation:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void UpdatesStarted() OVERRIDE;
  virtual void UpdatesFinished() OVERRIDE;
  virtual void UpdateAccountChooser() OVERRIDE;
  virtual void UpdateButtonStrip() OVERRIDE;
  virtual void UpdateOverlay() OVERRIDE;
  virtual void UpdateDetailArea() OVERRIDE;
  virtual void UpdateForErrors() OVERRIDE;
  virtual void UpdateNotificationArea() OVERRIDE;
  virtual void UpdateSection(DialogSection section) OVERRIDE;
  virtual void UpdateErrorBubble() OVERRIDE;
  virtual void FillSection(DialogSection section,
                           const DetailInput& originating_input) OVERRIDE;
  virtual void GetUserInput(DialogSection section,
                            DetailOutputMap* output) OVERRIDE;
  virtual string16 GetCvc() OVERRIDE;
  virtual bool HitTestInput(const DetailInput& input,
                            const gfx::Point& screen_point) OVERRIDE;
  virtual bool SaveDetailsLocally() OVERRIDE;
  virtual const content::NavigationController* ShowSignIn() OVERRIDE;
  virtual void HideSignIn() OVERRIDE;
  virtual void ModelChanged() OVERRIDE;
  virtual TestableAutofillDialogView* GetTestableView() OVERRIDE;
  virtual void OnSignInResize(const gfx::Size& pref_size) OVERRIDE;

  // TestableAutofillDialogView implementation:
  // TODO(groby): Create a separate class to implement the testable interface:
  // http://crbug.com/256864
  virtual void SubmitForTesting() OVERRIDE;
  virtual void CancelForTesting() OVERRIDE;
  virtual string16 GetTextContentsOfInput(const DetailInput& input) OVERRIDE;
  virtual void SetTextContentsOfInput(const DetailInput& input,
                                      const string16& contents) OVERRIDE;
  virtual void SetTextContentsOfSuggestionInput(
      DialogSection section,
      const base::string16& text) OVERRIDE;
  virtual void ActivateInput(const DetailInput& input) OVERRIDE;
  virtual gfx::Size GetSize() const OVERRIDE;
  virtual content::WebContents* GetSignInWebContents() OVERRIDE;
  virtual bool IsShowingOverlay() const OVERRIDE;

  // ConstrainedWindowMacDelegate implementation:
  virtual void OnConstrainedWindowClosed(
      ConstrainedWindowMac* window) OVERRIDE;

  AutofillDialogViewDelegate* delegate() { return delegate_; }

  // Posts a close request on the current message loop.
  void PerformClose();

 private:
  // Closes the sheet and ends the modal loop. Triggers cleanup sequence.
  void CloseNow();

  scoped_ptr<ConstrainedWindowMac> constrained_window_;
  base::scoped_nsobject<AutofillDialogWindowController> sheet_delegate_;

  // WeakPtrFactory for deferred close.
  base::WeakPtrFactory<AutofillDialogCocoa> close_weak_ptr_factory_;

  // The delegate |this| queries for logic and state.
  AutofillDialogViewDelegate* delegate_;
};

}  // autofill

@interface AutofillDialogWindowController :
    NSWindowController<NSWindowDelegate, AutofillLayout> {
 @private
  content::WebContents* webContents_;  // weak.
  autofill::AutofillDialogCocoa* autofillDialog_;  // weak.

  base::scoped_nsobject<AutofillMainContainer> mainContainer_;
  base::scoped_nsobject<AutofillSignInContainer> signInContainer_;
  base::scoped_nsobject<AutofillAccountChooser> accountChooser_;
  base::scoped_nsobject<AutofillOverlayController> overlayController_;
  base::scoped_nsobject<NSTextField> loadingShieldTextField_;
  base::scoped_nsobject<NSTextField> titleTextField_;
}

// Designated initializer. The WebContents cannot be NULL.
- (id)initWithWebContents:(content::WebContents*)webContents
           autofillDialog:(autofill::AutofillDialogCocoa*)autofillDialog;

// A child view request re-layouting.
- (void)requestRelayout;

// Cancels all previous requests to re-layout.
- (void)cancelRelayout;

// Validate data. If it is valid, notify the delegate that the user would
// like to use the data.
- (IBAction)accept:(id)sender;

// User cancels dialog.
- (IBAction)cancel:(id)sender;

// Forwarding AutofillDialogView calls.
- (void)show;
- (void)hide;
- (void)updateNotificationArea;
- (void)updateAccountChooser;
- (void)updateButtonStrip;
- (void)updateSection:(autofill::DialogSection)section;
- (void)fillSection:(autofill::DialogSection)section
           forInput:(const autofill::DetailInput&)input;
- (void)getInputs:(autofill::DetailOutputMap*)outputs
       forSection:(autofill::DialogSection)section;
- (NSString*)getCvc;
- (BOOL)saveDetailsLocally;
- (content::NavigationController*)showSignIn;
- (void)hideSignIn;
- (void)modelChanged;
- (void)updateErrorBubble;
- (void)onSignInResize:(NSSize)size;

@end


// Mirrors the TestableAutofillDialogView API on the C++ side.
@interface AutofillDialogWindowController (TestableAutofillDialogView)

- (void)setTextContents:(NSString*)text
               forInput:(const autofill::DetailInput&)input;
- (void)setTextContents:(NSString*)text
 ofSuggestionForSection:(autofill::DialogSection)section;
- (void)activateFieldForInput:(const autofill::DetailInput&)input;
- (content::WebContents*)getSignInWebContents;
- (BOOL)IsShowingOverlay;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_COCOA_H_
