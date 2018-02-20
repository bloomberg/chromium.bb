// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_mac.h"

#include "base/mac/availability.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/web_textfield_touch_bar_controller.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/popup_item_ids.h"

using base::WeakPtr;

namespace autofill {

// static
WeakPtr<AutofillPopupControllerImpl> AutofillPopupControllerImpl::GetOrCreate(
    WeakPtr<AutofillPopupControllerImpl> previous,
    WeakPtr<AutofillPopupDelegate> delegate,
    content::WebContents* web_contents,
    gfx::NativeView container_view,
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction) {
  if (previous.get() && previous->delegate_.get() == delegate.get() &&
      previous->container_view() == container_view) {
    previous->SetElementBounds(element_bounds);
    previous->ClearState();
    return previous;
  }

  if (previous.get())
    previous->Hide();

  AutofillPopupControllerImpl* controller = new AutofillPopupControllerImplMac(
      delegate, web_contents, container_view, element_bounds, text_direction);
  return controller->GetWeakPtr();
}

AutofillPopupControllerImplMac::AutofillPopupControllerImplMac(
    base::WeakPtr<AutofillPopupDelegate> delegate,
    content::WebContents* web_contents,
    gfx::NativeView container_view,
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction)
    : AutofillPopupControllerImpl(delegate,
                                  web_contents,
                                  container_view,
                                  element_bounds,
                                  text_direction),
      touchBarController_(nil) {}

AutofillPopupControllerImplMac::~AutofillPopupControllerImplMac() {}

void AutofillPopupControllerImplMac::Show(
    const std::vector<autofill::Suggestion>& suggestions) {
  AutofillPopupControllerImpl::Show(suggestions);

  if (!autofill::IsCreditCardAutofillTouchBarExperimentEnabled())
    return;

  if (!GetLineCount() || !layout_model().is_credit_card_popup())
    return;

  if (@available(macOS 10.12.2, *)) {
    BrowserWindowController* bwc = [BrowserWindowController
        browserWindowControllerForWindow:[container_view() window]];
    TabContentsController* tabContentsController =
        [[bwc tabStripController] activeTabContentsController];
    touchBarController_ =
        [tabContentsController webTextfieldTouchBarController];

    [touchBarController_ showCreditCardAutofillWithController:this];
  }
}

void AutofillPopupControllerImplMac::UpdateDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  AutofillPopupControllerImpl::UpdateDataListValues(values, labels);
  if (touchBarController_) {
    [touchBarController_ invalidateTouchBar];
  }
}

void AutofillPopupControllerImplMac::Hide() {
  if (touchBarController_) {
    [touchBarController_ hideCreditCardAutofillTouchBar];
    touchBarController_ = nil;
  }

  AutofillPopupControllerImpl::Hide();
}

}  // namespace autofill
