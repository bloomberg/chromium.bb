// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_INTERACTIVE_UITEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_INTERACTIVE_UITEST_BASE_H_

#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/payments/payment_request.mojom.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Widget;
}

namespace payments {

class PaymentRequest;

// Base class for any interactive PaymentRequest test that will need to open
// the UI and interact with it.
class PaymentRequestInteractiveTestBase
    : public InProcessBrowserTest,
      public PaymentRequestDialogView::ObserverForTest,
      public views::WidgetObserver {
 protected:
  // Test will open a browser window to |test_file_path| (relative to
  // chrome/test/data/payments).
  explicit PaymentRequestInteractiveTestBase(const std::string& test_file_path);
  ~PaymentRequestInteractiveTestBase() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  // PaymentRequestDialogView::ObserverForTest
  void OnDialogOpened() override;

  // views::WidgetObserver
  // Effective way to be warned of all dialog closures.
  void OnWidgetDestroyed(views::Widget* widget) override;

  // Will call JavaScript to invoke the PaymentRequest dialog and verify that
  // it's open.
  void InvokePaymentRequestUI();

  // Convenience method to get a list of PaymentRequest associated with
  // |web_contents|.
  const std::vector<PaymentRequest*> GetPaymentRequests(
      content::WebContents* web_contents);

  content::WebContents* GetActiveWebContents();

  void CreatePaymentRequestForTest(
      content::WebContents* web_contents,
      mojo::InterfaceRequest<payments::mojom::PaymentRequest> request);

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  // Various events that can be waited on by the DialogEventObserver.
  enum DialogEvent {
    DIALOG_OPENED,
    DIALOG_CLOSED,
  };

  // DialogEventObserver is used to wait on specific events that may have
  // occured before the call to Wait(), or after, in which case a RunLoop is
  // used.
  //
  // Usage:
  // observer_.reset(new DialogEventObserver([DialogEvent]));
  //
  // Do stuff, which (a)synchronously calls observer_->Observe([DialogEvent]).
  //
  // observer_->Wait();  <- Will either return right away if event was observed,
  //                     <- or use a RunLoop's Run/Quit to wait for the event.
  class DialogEventObserver {
   public:
    explicit DialogEventObserver(DialogEvent event);
    ~DialogEventObserver();

    // Either returns right away if the event was observed between this object's
    // construction and this call to Wait(), or use a RunLoop to wait for it.
    void Wait();

    // Observes the event (quits the RunLoop if it was running).
    void Observe(DialogEvent event);

   private:
    DialogEvent event_;
    bool seen_;
    base::RunLoop run_loop_;

    DISALLOW_COPY_AND_ASSIGN(DialogEventObserver);
  };

  // Resets the event observer for a given |event|.
  void ResetEventObserver(DialogEvent event);
  // Wait for the event passed to ResetEventObserver() to occur.
  void WaitForObservedEvent();

 private:
  std::unique_ptr<DialogEventObserver> event_observer_;
  const std::string test_file_path_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestInteractiveTestBase);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_INTERACTIVE_UITEST_BASE_H_
