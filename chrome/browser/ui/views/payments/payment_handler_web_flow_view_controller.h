// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_

#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/separator.h"
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
      public content::WebContentsDelegate,
      public content::WebContentsObserver {
 public:
  // This ctor forwards its first 3 args to PaymentRequestSheetController's
  // ctor. |log_destination| is the page whose web developer console will print
  // error messages. That should be the page that instantiated PaymentRequest
  // for developer convinience. |profile| is the browser context used to create
  // the new WebContents object that will navigate to |target|.
  // |first_navigation_complete_callback| is invoked once the WebContents
  // finishes the initial navigation to |target|.
  PaymentHandlerWebFlowViewController(
      PaymentRequestSpec* spec,
      PaymentRequestState* state,
      PaymentRequestDialogView* dialog,
      content::WebContents* log_destination,
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
  views::View* CreateHeaderContentSeparatorView() override;
  std::unique_ptr<views::Background> GetHeaderBackground() override;
  bool GetSheetId(DialogViewID* sheet_id) override;
  bool DisplayDynamicBorderForHiddenContents() override;

  // content::WebContentsDelegate:
  void LoadProgressChanged(content::WebContents* source,
                           double progress) override;
  void VisibleSecurityStateChanged(content::WebContents* source) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidAttachInterstitialPage() override;

  void AbortPayment();

  DeveloperConsoleLogger log_;
  Profile* profile_;
  GURL target_;
  bool show_progress_bar_;
  std::unique_ptr<views::ProgressBar> progress_bar_;
  std::unique_ptr<views::Separator> separator_;
  PaymentHandlerOpenWindowCallback first_navigation_complete_callback_;
  base::string16 https_prefix_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_
