// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_INTERACTIVE_UITEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_INTERACTIVE_UITEST_BASE_H_

#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/test_chrome_payment_request_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/payments/content/payment_request.mojom.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/widget_observer.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Widget;
}

namespace payments {

enum class DialogViewID;
class PaymentRequest;

namespace {

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

}  // namespace

class PersonalDataLoadedObserverMock
    : public autofill::PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock();
  ~PersonalDataLoadedObserverMock() override;

  MOCK_METHOD0(OnPersonalDataChanged, void());
};

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
  void OnOrderSummaryOpened() override;
  void OnPaymentMethodOpened() override;
  void OnCreditCardEditorOpened() override;
  void OnBackNavigation() override;
  void OnContactInfoOpened() override;

  // views::WidgetObserver
  // Effective way to be warned of all dialog closures.
  void OnWidgetDestroyed(views::Widget* widget) override;

  // Will call JavaScript to invoke the PaymentRequest dialog and verify that
  // it's open.
  void InvokePaymentRequestUI();

  // Utility functions that will click on Dialog views and wait for the
  // associated action to happen.
  void OpenOrderSummaryScreen();
  void OpenPaymentMethodScreen();
  void OpenCreditCardEditorScreen();

  content::WebContents* GetActiveWebContents();

  // Convenience method to get a list of PaymentRequest associated with
  // |web_contents|.
  const std::vector<PaymentRequest*> GetPaymentRequests(
      content::WebContents* web_contents);

  autofill::PersonalDataManager* GetDataManager();
  // Adds the various models to the database, waiting until the personal data
  // manager notifies that they are added.
  // NOTE: If no use_count is specified on the models and multiple items are
  // inserted, the order in which they are returned is undefined, since they
  // are added close to each other.
  void AddAutofillProfile(const autofill::AutofillProfile& profile);
  void AddCreditCard(const autofill::CreditCard& card);

  void CreatePaymentRequestForTest(
      content::WebContents* web_contents,
      mojo::InterfaceRequest<payments::mojom::PaymentRequest> request);

  // Click on a view from within the dialog and waits for an observed event
  // to be observed.
  void ClickOnDialogViewAndWait(DialogViewID view_id);
  void ClickOnDialogViewAndWait(views::View* view);

  // Setting the |value| in the textfield of a given |type|.
  void SetEditorTextfieldValue(const base::string16& value,
                               autofill::ServerFieldType type);
  // Setting the |value| in the combobox of a given |type|.
  void SetComboboxValue(const base::string16& value,
                        autofill::ServerFieldType type);

  // Whether the editor textfield/combobox for the given |type| is currently in
  // an invalid state.
  bool IsEditorTextfieldInvalid(autofill::ServerFieldType type);
  bool IsEditorComboboxInvalid(autofill::ServerFieldType type);

  bool IsPayButtonEnabled();

  // Sets proper animation delegates and waits for animation to finish.
  void WaitForAnimation();

  // Returns the text of the StyledLabel with the specific |view_id| that is a
  // child of the Payment Request dialog view.
  const base::string16& GetStyledLabelText(DialogViewID view_id);
  // Returns the error label text associated with a given field |type|.
  const base::string16& GetErrorLabelForType(autofill::ServerFieldType type);

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  PaymentRequestDialogView* dialog_view() { return delegate_->dialog_view(); }

  // Various events that can be waited on by the DialogEventObserver.
  enum DialogEvent : int {
    DIALOG_OPENED,
    DIALOG_CLOSED,
    ORDER_SUMMARY_OPENED,
    PAYMENT_METHOD_OPENED,
    CREDIT_CARD_EDITOR_OPENED,
    BACK_NAVIGATION,
    CONTACT_INFO_OPENED,
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
  // Weak, owned by the PaymentRequest object.
  TestChromePaymentRequestDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestInteractiveTestBase);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_INTERACTIVE_UITEST_BASE_H_
