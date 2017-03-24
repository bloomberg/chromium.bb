// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "chrome/browser/ui/views/payments/view_stack.h"
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
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/test_animation_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"

namespace payments {

PersonalDataLoadedObserverMock::PersonalDataLoadedObserverMock() {}
PersonalDataLoadedObserverMock::~PersonalDataLoadedObserverMock() {}

PaymentRequestBrowserTestBase::PaymentRequestBrowserTestBase(
    const std::string& test_file_path)
    : test_file_path_(test_file_path),
      delegate_(nullptr),
      incognito_for_testing_(false) {}
PaymentRequestBrowserTestBase::~PaymentRequestBrowserTestBase() {}

void PaymentRequestBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  InProcessBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  features::kWebPayments.name);
}

void PaymentRequestBrowserTestBase::SetUpOnMainThread() {
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
  registry->AddInterface(
      base::Bind(&PaymentRequestBrowserTestBase::CreatePaymentRequestForTest,
                 base::Unretained(this), web_contents));
}

void PaymentRequestBrowserTestBase::SetIncognitoForTesting() {
  incognito_for_testing_ = true;
}

void PaymentRequestBrowserTestBase::OnDialogOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::DIALOG_OPENED);
}

void PaymentRequestBrowserTestBase::OnOrderSummaryOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::ORDER_SUMMARY_OPENED);
}

void PaymentRequestBrowserTestBase::OnPaymentMethodOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::PAYMENT_METHOD_OPENED);
}

void PaymentRequestBrowserTestBase::OnShippingSectionOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::SHIPPING_SECTION_OPENED);
}

void PaymentRequestBrowserTestBase::OnCreditCardEditorOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::CREDIT_CARD_EDITOR_OPENED);
}

void PaymentRequestBrowserTestBase::OnShippingAddressEditorOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
}

void PaymentRequestBrowserTestBase::OnBackNavigation() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::BACK_NAVIGATION);
}

void PaymentRequestBrowserTestBase::OnContactInfoOpened() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::CONTACT_INFO_OPENED);
}

void PaymentRequestBrowserTestBase::OnEditorViewUpdated() {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::EDITOR_VIEW_UPDATED);
}

void PaymentRequestBrowserTestBase::OnWidgetDestroyed(views::Widget* widget) {
  if (event_observer_)
    event_observer_->Observe(DialogEvent::DIALOG_CLOSED);
}

void PaymentRequestBrowserTestBase::InvokePaymentRequestUI() {
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

void PaymentRequestBrowserTestBase::ExpectBodyContains(
    const std::vector<base::string16>& expected_strings) {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string extract_contents_js =
      "(function() { "
      "window.domAutomationController.send(window.document.body.textContent); "
      "})()";
  std::string contents;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, extract_contents_js, &contents));
  for (const auto expected_string : expected_strings) {
    EXPECT_NE(std::string::npos,
              contents.find(base::UTF16ToUTF8(expected_string)))
        << "String not present: " << expected_string;
  }
}

void PaymentRequestBrowserTestBase::OpenOrderSummaryScreen() {
  ResetEventObserver(DialogEvent::ORDER_SUMMARY_OPENED);

  ClickOnDialogViewAndWait(DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION);
}

void PaymentRequestBrowserTestBase::OpenPaymentMethodScreen() {
  ResetEventObserver(DialogEvent::PAYMENT_METHOD_OPENED);

  ClickOnDialogViewAndWait(DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION);
}

void PaymentRequestBrowserTestBase::OpenShippingSectionScreen() {
  ResetEventObserver(DialogEvent::SHIPPING_SECTION_OPENED);

  ClickOnDialogViewAndWait(DialogViewID::PAYMENT_SHEET_SHIPPING_SECTION);
}

void PaymentRequestBrowserTestBase::OpenCreditCardEditorScreen() {
  ResetEventObserver(DialogEvent::CREDIT_CARD_EDITOR_OPENED);

  ClickOnDialogViewAndWait(DialogViewID::PAYMENT_METHOD_ADD_CARD_BUTTON);
}

void PaymentRequestBrowserTestBase::OpenShippingAddressEditorScreen() {
  ResetEventObserver(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);

  ClickOnDialogViewAndWait(DialogViewID::PAYMENT_METHOD_ADD_SHIPPING_BUTTON);
}

content::WebContents* PaymentRequestBrowserTestBase::GetActiveWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

const std::vector<PaymentRequest*>
PaymentRequestBrowserTestBase::GetPaymentRequests(
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

autofill::PersonalDataManager* PaymentRequestBrowserTestBase::GetDataManager() {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()));
}

void PaymentRequestBrowserTestBase::AddAutofillProfile(
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

void PaymentRequestBrowserTestBase::AddCreditCard(
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

void PaymentRequestBrowserTestBase::CreatePaymentRequestForTest(
    content::WebContents* web_contents,
    mojo::InterfaceRequest<payments::mojom::PaymentRequest> request) {
  DCHECK(web_contents);
  std::unique_ptr<TestChromePaymentRequestDelegate> delegate =
      base::MakeUnique<TestChromePaymentRequestDelegate>(
          web_contents, this /* observer */, this /* widget_observer */,
          incognito_for_testing_);
  delegate_ = delegate.get();
  PaymentRequestWebContentsManager::GetOrCreateForWebContents(web_contents)
      ->CreatePaymentRequest(web_contents, std::move(delegate),
                             std::move(request));
}

void PaymentRequestBrowserTestBase::ClickOnDialogViewAndWait(
    DialogViewID view_id) {
  views::View* view =
      delegate_->dialog_view()->GetViewByID(static_cast<int>(view_id));
  DCHECK(view);
  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::ClickOnDialogViewAndWait(
    views::View* view) {
  DCHECK(view);
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMousePressed(pressed);
  ui::MouseEvent released_event = ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMouseReleased(released_event);

  WaitForAnimation();

  WaitForObservedEvent();
}

void PaymentRequestBrowserTestBase::SetEditorTextfieldValue(
    const base::string16& value,
    autofill::ServerFieldType type) {
  ValidatingTextfield* textfield = static_cast<ValidatingTextfield*>(
      delegate_->dialog_view()->GetViewByID(static_cast<int>(type)));
  DCHECK(textfield);
  textfield->SetText(value);
  textfield->OnContentsChanged();
  textfield->OnBlur();
}

void PaymentRequestBrowserTestBase::SetComboboxValue(
    const base::string16& value,
    autofill::ServerFieldType type) {
  ValidatingCombobox* combobox = static_cast<ValidatingCombobox*>(
      delegate_->dialog_view()->GetViewByID(static_cast<int>(type)));
  DCHECK(combobox);
  combobox->SelectValue(value);
  combobox->OnContentsChanged();
  combobox->OnBlur();
}

bool PaymentRequestBrowserTestBase::IsEditorTextfieldInvalid(
    autofill::ServerFieldType type) {
  ValidatingTextfield* textfield = static_cast<ValidatingTextfield*>(
      delegate_->dialog_view()->GetViewByID(static_cast<int>(type)));
  DCHECK(textfield);
  return textfield->invalid();
}

bool PaymentRequestBrowserTestBase::IsEditorComboboxInvalid(
    autofill::ServerFieldType type) {
  ValidatingCombobox* combobox = static_cast<ValidatingCombobox*>(
      delegate_->dialog_view()->GetViewByID(static_cast<int>(type)));
  DCHECK(combobox);
  return combobox->invalid();
}

bool PaymentRequestBrowserTestBase::IsPayButtonEnabled() {
  views::Button* button =
      static_cast<views::Button*>(delegate_->dialog_view()->GetViewByID(
          static_cast<int>(DialogViewID::PAY_BUTTON)));
  DCHECK(button);
  return button->enabled();
}

void PaymentRequestBrowserTestBase::WaitForAnimation() {
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

const base::string16& PaymentRequestBrowserTestBase::GetStyledLabelText(
    DialogViewID view_id) {
  views::View* view = dialog_view()->GetViewByID(static_cast<int>(view_id));
  DCHECK(view);
  return static_cast<views::StyledLabel*>(view)->text();
}

const base::string16& PaymentRequestBrowserTestBase::GetErrorLabelForType(
    autofill::ServerFieldType type) {
  views::View* view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::ERROR_LABEL_OFFSET) + type);
  DCHECK(view);
  return static_cast<views::Label*>(view)->text();
}

PaymentRequestBrowserTestBase::DialogEventObserver::DialogEventObserver(
    PaymentRequestBrowserTestBase::DialogEvent event)
    : event_(event), seen_(false) {}
PaymentRequestBrowserTestBase::DialogEventObserver::~DialogEventObserver() {}

void PaymentRequestBrowserTestBase::DialogEventObserver::Wait() {
  if (seen_)
    return;

  DCHECK(!run_loop_.running());
  run_loop_.Run();
}

void PaymentRequestBrowserTestBase::DialogEventObserver::Observe(
    PaymentRequestBrowserTestBase::DialogEvent event) {
  if (seen_)
    return;

  DCHECK_EQ(event_, event);
  seen_ = true;
  if (run_loop_.running())
    run_loop_.Quit();
}

void PaymentRequestBrowserTestBase::ResetEventObserver(DialogEvent event) {
  event_observer_ = base::MakeUnique<DialogEventObserver>(event);
}

void PaymentRequestBrowserTestBase::WaitForObservedEvent() {
  event_observer_->Wait();
}

}  // namespace payments
