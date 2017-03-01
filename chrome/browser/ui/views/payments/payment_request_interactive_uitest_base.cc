// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_interactive_uitest_base.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "chrome/browser/ui/views/payments/view_stack.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/animation/test_animation_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"

namespace payments {

PersonalDataLoadedObserverMock::PersonalDataLoadedObserverMock() {}
PersonalDataLoadedObserverMock::~PersonalDataLoadedObserverMock() {}

PaymentRequestInteractiveTestBase::PaymentRequestInteractiveTestBase(
    const std::string& test_file_path)
    : test_file_path_(test_file_path),
      delegate_(nullptr) {}
PaymentRequestInteractiveTestBase::~PaymentRequestInteractiveTestBase() {}

void PaymentRequestInteractiveTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  InProcessBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  features::kWebPayments.name);
}

void PaymentRequestInteractiveTestBase::SetUpOnMainThread() {
  https_server_ = base::MakeUnique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server_->InitializeAndListen());
  https_server_->ServeFilesFromSourceDirectory("chrome/test/data/payments");
  https_server_->StartAcceptingConnections();

  GURL url = https_server()->GetURL(test_file_path_);
  ui_test_utils::NavigateToURL(browser(), url);

  // Starting now, PaymentRequest Mojo messages sent by the renderer will
  // create PaymentRequest objects via this test's CreatePaymentRequestForTest,
  // allowing the test to inject itself as a dialog observer.
  content::WebContents* web_contents = GetActiveWebContents();
  service_manager::InterfaceRegistry* registry =
      web_contents->GetMainFrame()->GetInterfaceRegistry();
  registry->RemoveInterface(payments::mojom::PaymentRequest::Name_);
  registry->AddInterface(base::Bind(
      &PaymentRequestInteractiveTestBase::CreatePaymentRequestForTest,
      base::Unretained(this), web_contents));
}

void PaymentRequestInteractiveTestBase::OnDialogOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::DIALOG_OPENED);
}

void PaymentRequestInteractiveTestBase::OnOrderSummaryOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::ORDER_SUMMARY_OPENED);
}

void PaymentRequestInteractiveTestBase::OnPaymentMethodOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::PAYMENT_METHOD_OPENED);
}

void PaymentRequestInteractiveTestBase::OnCreditCardEditorOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::CREDIT_CARD_EDITOR_OPENED);
}

void PaymentRequestInteractiveTestBase::OnBackNavigation() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::BACK_NAVIGATION);
}

void PaymentRequestInteractiveTestBase::OnContactInfoOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::CONTACT_INFO_OPENED);
}

void PaymentRequestInteractiveTestBase::OnWidgetDestroyed(
    views::Widget* widget) {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::DIALOG_CLOSED);
}

void PaymentRequestInteractiveTestBase::InvokePaymentRequestUI() {
  ResetEventObserver(DialogEvent::DIALOG_OPENED);

  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('buy').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));

  WaitForObservedEvent();

  // The web-modal dialog should be open.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());
}

void PaymentRequestInteractiveTestBase::OpenOrderSummaryScreen() {
  ResetEventObserver(DialogEvent::ORDER_SUMMARY_OPENED);

  ClickOnDialogViewAndWait(DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION);
}

void PaymentRequestInteractiveTestBase::OpenPaymentMethodScreen() {
  ResetEventObserver(DialogEvent::PAYMENT_METHOD_OPENED);

  ClickOnDialogViewAndWait(DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION);
}

void PaymentRequestInteractiveTestBase::OpenCreditCardEditorScreen() {
  ResetEventObserver(DialogEvent::CREDIT_CARD_EDITOR_OPENED);

  ClickOnDialogViewAndWait(DialogViewID::PAYMENT_METHOD_ADD_CARD_BUTTON);
}

content::WebContents*
PaymentRequestInteractiveTestBase::GetActiveWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

const std::vector<PaymentRequest*>
PaymentRequestInteractiveTestBase::GetPaymentRequests(
    content::WebContents* web_contents) {
  PaymentRequestWebContentsManager* manager =
      PaymentRequestWebContentsManager::GetOrCreateForWebContents(web_contents);
  if (!manager)
    return std::vector<PaymentRequest*>();

  std::vector<PaymentRequest*> payment_requests_ptrs;
  for (const auto& p : manager->payment_requests_)
    payment_requests_ptrs.push_back(p.first);
  return payment_requests_ptrs;
}

autofill::PersonalDataManager*
PaymentRequestInteractiveTestBase::GetDataManager() {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()));
}

void PaymentRequestInteractiveTestBase::AddAutofillProfile(
    const autofill::AutofillProfile& profile) {
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  size_t profile_count = personal_data_manager->GetProfiles().size();

  PersonalDataLoadedObserverMock personal_data_observer;
  personal_data_manager->AddObserver(&personal_data_observer);
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  personal_data_manager->AddProfile(profile);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer);
  EXPECT_EQ(profile_count + 1, personal_data_manager->GetProfiles().size());
}

void PaymentRequestInteractiveTestBase::AddCreditCard(
    const autofill::CreditCard& card) {
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  size_t card_count = personal_data_manager->GetCreditCards().size();

  PersonalDataLoadedObserverMock personal_data_observer;
  personal_data_manager->AddObserver(&personal_data_observer);
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  personal_data_manager->AddCreditCard(card);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer);
  EXPECT_EQ(card_count + 1, personal_data_manager->GetCreditCards().size());
}

void PaymentRequestInteractiveTestBase::CreatePaymentRequestForTest(
    content::WebContents* web_contents,
    mojo::InterfaceRequest<payments::mojom::PaymentRequest> request) {
  DCHECK(web_contents);
  std::unique_ptr<TestChromePaymentRequestDelegate> delegate =
      base::MakeUnique<TestChromePaymentRequestDelegate>(
          web_contents, this /* observer */, this /* widget_observer */);
  delegate_ = delegate.get();
  PaymentRequestWebContentsManager::GetOrCreateForWebContents(web_contents)
      ->CreatePaymentRequest(web_contents, std::move(delegate),
                             std::move(request));
}

void PaymentRequestInteractiveTestBase::ClickOnDialogViewAndWait(
    DialogViewID view_id) {
  views::View* view =
      delegate_->dialog_view()->GetViewByID(static_cast<int>(view_id));
  DCHECK(view);
  ClickOnDialogViewAndWait(view);
}

void PaymentRequestInteractiveTestBase::ClickOnDialogViewAndWait(
    views::View* view) {
  DCHECK(view);
  base::RunLoop run_loop;
  ui_test_utils::MoveMouseToCenterAndPress(
      view, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
      run_loop.QuitClosure());
  run_loop.Run();

  WaitForAnimation();

  WaitForObservedEvent();
}

void PaymentRequestInteractiveTestBase::SetEditorTextfieldValue(
    const base::string16& value,
    autofill::ServerFieldType type) {
  ValidatingTextfield* textfield = static_cast<ValidatingTextfield*>(
      delegate_->dialog_view()->GetViewByID(static_cast<int>(type)));
  DCHECK(textfield);
  textfield->SetText(value);
  textfield->OnContentsChanged();
  textfield->OnBlur();
}

void PaymentRequestInteractiveTestBase::SetComboboxValue(
    const base::string16& value,
    autofill::ServerFieldType type) {
  ValidatingCombobox* combobox = static_cast<ValidatingCombobox*>(
      delegate_->dialog_view()->GetViewByID(static_cast<int>(type)));
  DCHECK(combobox);
  combobox->SelectValue(value);
  combobox->OnContentsChanged();
  combobox->OnBlur();
}

bool PaymentRequestInteractiveTestBase::IsEditorTextfieldInvalid(
    autofill::ServerFieldType type) {
  ValidatingTextfield* textfield = static_cast<ValidatingTextfield*>(
      delegate_->dialog_view()->GetViewByID(static_cast<int>(type)));
  DCHECK(textfield);
  return textfield->invalid();
}

bool PaymentRequestInteractiveTestBase::IsEditorComboboxInvalid(
    autofill::ServerFieldType type) {
  ValidatingCombobox* combobox = static_cast<ValidatingCombobox*>(
      delegate_->dialog_view()->GetViewByID(static_cast<int>(type)));
  DCHECK(combobox);
  return combobox->invalid();
}

bool PaymentRequestInteractiveTestBase::IsPayButtonEnabled() {
  views::Button* button =
      static_cast<views::Button*>(delegate_->dialog_view()->GetViewByID(
          static_cast<int>(DialogViewID::PAY_BUTTON)));
  DCHECK(button);
  return button->enabled();
}

void PaymentRequestInteractiveTestBase::WaitForAnimation() {
  ViewStack* view_stack = dialog_view()->view_stack_for_testing();
  if (view_stack->slide_in_animator_->IsAnimating()) {
    view_stack->slide_in_animator_->SetAnimationDuration(1);
    view_stack->slide_in_animator_->SetAnimationDelegate(
        view_stack->top(), std::unique_ptr<gfx::AnimationDelegate>(
                               new gfx::TestAnimationDelegate()));
    base::RunLoop().Run();
  } else if (view_stack->slide_out_animator_->IsAnimating()) {
    view_stack->slide_out_animator_->SetAnimationDuration(1);
    view_stack->slide_out_animator_->SetAnimationDelegate(
        view_stack->top(), std::unique_ptr<gfx::AnimationDelegate>(
                               new gfx::TestAnimationDelegate()));
    base::RunLoop().Run();
  }
}

const base::string16& PaymentRequestInteractiveTestBase::GetStyledLabelText(
    DialogViewID view_id) {
  views::View* view = dialog_view()->GetViewByID(static_cast<int>(view_id));
  DCHECK(view);
  return static_cast<views::StyledLabel*>(view)->text();
}

const base::string16& PaymentRequestInteractiveTestBase::GetErrorLabelForType(
    autofill::ServerFieldType type) {
  views::View* view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::ERROR_LABEL_OFFSET) + type);
  DCHECK(view);
  return static_cast<views::Label*>(view)->text();
}

PaymentRequestInteractiveTestBase::DialogEventObserver::DialogEventObserver(
    PaymentRequestInteractiveTestBase::DialogEvent event)
    : event_(event), seen_(false) {}
PaymentRequestInteractiveTestBase::DialogEventObserver::~DialogEventObserver() {
}

void PaymentRequestInteractiveTestBase::DialogEventObserver::Wait() {
  if (seen_)
    return;

  DCHECK(!run_loop_.running());
  run_loop_.Run();
}

void PaymentRequestInteractiveTestBase::DialogEventObserver::Observe(
    PaymentRequestInteractiveTestBase::DialogEvent event) {
  if (seen_)
    return;

  DCHECK_EQ(event_, event);
  seen_ = true;
  if (run_loop_.running())
    run_loop_.Quit();
}

void PaymentRequestInteractiveTestBase::ResetEventObserver(DialogEvent event) {
  event_observer_ = base::MakeUnique<DialogEventObserver>(event);
}

void PaymentRequestInteractiveTestBase::WaitForObservedEvent() {
  event_observer_->Wait();
}

}  // namespace payments
