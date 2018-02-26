// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_

#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

class Profile;

namespace payments {

class PaymentRequestDialogView;
class PaymentRequestSpec;
class PaymentRequestState;

// Displays a screen in the Payment Request dialog that shows the web page at
// |target| inside a views::WebView control.
class PaymentHandlerWebFlowViewController
    : public PaymentRequestSheetController,
      public content::WebContentsObserver {
 public:
  // This ctor forwards its first 3 args to PaymentRequestSheetController's
  // ctor. |profile| is the browser context used to create the WebContents
  // object that will navigate to |target|. |first_navigation_complete_callback|
  // is invoked once the WebContents finishes the initial navigation to
  // |target|.
  PaymentHandlerWebFlowViewController(
      PaymentRequestSpec* spec,
      PaymentRequestState* state,
      PaymentRequestDialogView* dialog,
      Profile* profile,
      GURL target,
      PaymentHandlerOpenWindowCallback first_navigation_complete_callback);
  ~PaymentHandlerWebFlowViewController() override;

 private:
  // PaymentRequestSheetController:
  base::string16 GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;
  bool ShouldShowSecondaryButton() override;
  std::unique_ptr<views::View> CreateHeaderContentView() override;
  std::unique_ptr<views::Background> GetHeaderBackground() override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

  Profile* profile_;
  GURL target_;
  PaymentHandlerOpenWindowCallback first_navigation_complete_callback_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_
