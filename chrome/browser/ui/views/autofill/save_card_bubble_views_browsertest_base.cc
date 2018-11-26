// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_bubble_views_browsertest_base.h"

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/save_card_icon_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/credit_card_save_manager.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/non_client_view.h"

namespace autofill {

namespace {

const char kURLGetUploadDetailsRequest[] =
    "https://payments.google.com/payments/apis/chromepaymentsservice/"
    "getdetailsforsavecard";
const char kResponseGetUploadDetailsSuccess[] =
    "{\"legal_message\":{\"line\":[{\"template\":\"Legal message template with "
    "link: "
    "{0}.\",\"template_parameter\":[{\"display_text\":\"Link\",\"url\":\"https:"
    "//www.example.com/\"}]}]},\"context_token\":\"dummy_context_token\"}";
const char kResponseGetUploadDetailsFailure[] =
    "{\"error\":{\"code\":\"FAILED_PRECONDITION\",\"user_error_message\":\"An "
    "unexpected error has occurred. Please try again later.\"}}";
const char kURLUploadCardRequest[] =
    "https://payments.google.com/payments/apis/chromepaymentsservice/savecard";

const double kFakeGeolocationLatitude = 1.23;
const double kFakeGeolocationLongitude = 4.56;

}  // namespace

SaveCardBubbleViewsBrowserTestBase::SaveCardBubbleViewsBrowserTestBase(
    const std::string& test_file_path)
    : SyncTest(SINGLE_CLIENT), test_file_path_(test_file_path) {}

SaveCardBubbleViewsBrowserTestBase::~SaveCardBubbleViewsBrowserTestBase() {}

void SaveCardBubbleViewsBrowserTestBase::SetUpOnMainThread() {
  SyncTest::SetUpOnMainThread();

  // Set up the HTTPS server (uses the embedded_test_server).
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "components/test/data/autofill");
  embedded_test_server()->StartAcceptingConnections();

  ProfileSyncServiceFactory::GetForProfile(browser()->profile())
      ->OverrideNetworkResourcesForTest(
          std::make_unique<fake_server::FakeServerNetworkResources>(
              GetFakeServer()->AsWeakPtr()));

  std::string username;
#if defined(OS_CHROMEOS)
  // In ChromeOS browser tests, the profile may already by authenticated with
  // stub account |user_manager::kStubUserEmail|.
  AccountInfo info = IdentityManagerFactory::GetForProfile(browser()->profile())
                         ->GetPrimaryAccountInfo();
  username = info.email;
#endif
  if (username.empty())
    username = "user@gmail.com";

  harness_ = ProfileSyncServiceHarness::Create(
      browser()->profile(), username, "password",
      ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);

  // Set up the URL loader factory for the payments client so we can intercept
  // those network requests too.
  test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  ContentAutofillDriver::GetForRenderFrameHost(
      GetActiveWebContents()->GetMainFrame())
      ->autofill_manager()
      ->client()
      ->GetPaymentsClient()
      ->set_url_loader_factory_for_testing(test_shared_loader_factory_);

  // Set up this class as the ObserverForTest implementation.
  CreditCardSaveManager* credit_card_save_manager =
      ContentAutofillDriver::GetForRenderFrameHost(
          GetActiveWebContents()->GetMainFrame())
          ->autofill_manager()
          ->client()
          ->GetFormDataImporter()
          ->credit_card_save_manager_.get();
  credit_card_save_manager->SetEventObserverForTesting(this);

  // Set up the fake geolocation data.
  geolocation_overrider_ = std::make_unique<device::ScopedGeolocationOverrider>(
      kFakeGeolocationLatitude, kFakeGeolocationLongitude);
}

void SaveCardBubbleViewsBrowserTestBase::NavigateTo(
    const std::string& file_path) {
  if (file_path.find("data:") == 0U) {
    ui_test_utils::NavigateToURL(browser(), GURL(file_path));
  } else {
    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL(file_path));
  }
}

void SaveCardBubbleViewsBrowserTestBase::OnOfferLocalSave() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::OFFERED_LOCAL_SAVE);
}

void SaveCardBubbleViewsBrowserTestBase::OnDecideToRequestUploadSave() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::REQUESTED_UPLOAD_SAVE);
}

void SaveCardBubbleViewsBrowserTestBase::OnReceivedGetUploadDetailsResponse() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE);
}

void SaveCardBubbleViewsBrowserTestBase::OnSentUploadCardRequest() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::SENT_UPLOAD_CARD_REQUEST);
}

void SaveCardBubbleViewsBrowserTestBase::OnReceivedUploadCardResponse() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::RECEIVED_UPLOAD_CARD_RESPONSE);
}

void SaveCardBubbleViewsBrowserTestBase::OnCCSMStrikeChangeComplete() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::STRIKE_CHANGE_COMPLETE);
}

void SaveCardBubbleViewsBrowserTestBase::OnBubbleShown() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::BUBBLE_SHOWN);
}

void SaveCardBubbleViewsBrowserTestBase::OnBubbleClosed() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::BUBBLE_CLOSED);
}

void SaveCardBubbleViewsBrowserTestBase::OnSCBCStrikeChangeComplete() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::STRIKE_CHANGE_COMPLETE);
}

void SaveCardBubbleViewsBrowserTestBase::SetAccountFullName(
    const std::string& full_name) {
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  AccountInfo info = identity_manager->GetPrimaryAccountInfo();
  info.full_name = full_name;
  identity::UpdateAccountInfoForAccount(identity_manager, info);
}

void SaveCardBubbleViewsBrowserTestBase::SubmitForm() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_submit_button_js =
      "(function() { document.getElementById('submit').click(); })();";
  content::TestNavigationObserver nav_observer(web_contents);
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_submit_button_js));
  nav_observer.Wait();
}

// Should be called for credit_card_upload_form_address_and_cc.html.
void SaveCardBubbleViewsBrowserTestBase::FillAndSubmitForm() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_fill_button_js =
      "(function() { document.getElementById('fill_form').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));
  SubmitForm();
}

void SaveCardBubbleViewsBrowserTestBase::
    FillAndSubmitFormWithCardDetailsOnly() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_fill_card_button_js =
      "(function() { document.getElementById('fill_card_only').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_card_button_js));

  SubmitForm();
}

// Should be called for credit_card_upload_form_address_and_cc.html.
void SaveCardBubbleViewsBrowserTestBase::FillAndSubmitFormWithoutCvc() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_fill_button_js =
      "(function() { document.getElementById('fill_form').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

  const std::string click_clear_cvc_button_js =
      "(function() { document.getElementById('clear_cvc').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_clear_cvc_button_js));

  SubmitForm();
}

// Should be called for credit_card_upload_form_address_and_cc.html.
void SaveCardBubbleViewsBrowserTestBase::FillAndSubmitFormWithInvalidCvc() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_fill_button_js =
      "(function() { document.getElementById('fill_form').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

  const std::string click_fill_invalid_cvc_button_js =
      "(function() { document.getElementById('fill_invalid_cvc').click(); "
      "})();";
  ASSERT_TRUE(
      content::ExecuteScript(web_contents, click_fill_invalid_cvc_button_js));

  SubmitForm();
}

// Should be called for credit_card_upload_form_address_and_cc.html.
void SaveCardBubbleViewsBrowserTestBase::FillAndSubmitFormWithoutName() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_fill_button_js =
      "(function() { document.getElementById('fill_form').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

  const std::string click_clear_name_button_js =
      "(function() { document.getElementById('clear_name').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_clear_name_button_js));

  SubmitForm();
}

// Should be called for credit_card_upload_form_shipping_address.html.
void SaveCardBubbleViewsBrowserTestBase::
    FillAndSubmitFormWithConflictingName() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_fill_button_js =
      "(function() { document.getElementById('fill_form').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

  const std::string click_conflicting_name_button_js =
      "(function() { document.getElementById('conflicting_name').click(); "
      "})();";
  ASSERT_TRUE(
      content::ExecuteScript(web_contents, click_conflicting_name_button_js));

  SubmitForm();
}

// Should be called for credit_card_upload_form_address_and_cc.html.
void SaveCardBubbleViewsBrowserTestBase::
    FillAndSubmitFormWithoutExpirationDate() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_fill_button_js =
      "(function() { document.getElementById('fill_form').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

  const std::string click_clear_expiration_date_button_js =
      "(function() { document.getElementById('clear_expiration_date').click(); "
      "})();";
  ASSERT_TRUE(content::ExecuteScript(web_contents,
                                     click_clear_expiration_date_button_js));

  SubmitForm();
}

// Should be called for credit_card_upload_form_address_and_cc.html.
void SaveCardBubbleViewsBrowserTestBase::
    FillAndSubmitFormWithExpiredExpirationDate() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_fill_button_js =
      "(function() { document.getElementById('fill_form').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

  const std::string click_fill_expired_expiration_date_button_js =
      "(function() { "
      "document.getElementById('fill_expired_expiration_date').click(); "
      "})();";
  ASSERT_TRUE(content::ExecuteScript(
      web_contents, click_fill_expired_expiration_date_button_js));

  SubmitForm();
}

// Should be called for credit_card_upload_form_address_and_cc.html.
void SaveCardBubbleViewsBrowserTestBase::FillAndSubmitFormWithoutAddress() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_fill_button_js =
      "(function() { document.getElementById('fill_form').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

  const std::string click_clear_address_button_js =
      "(function() { document.getElementById('clear_address').click(); })();";
  ASSERT_TRUE(
      content::ExecuteScript(web_contents, click_clear_address_button_js));

  SubmitForm();
}

// Should be called for credit_card_upload_form_shipping_address.html.
void SaveCardBubbleViewsBrowserTestBase::
    FillAndSubmitFormWithConflictingStreetAddress() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_fill_button_js =
      "(function() { document.getElementById('fill_form').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

  const std::string click_conflicting_street_address_button_js =
      "(function() { "
      "document.getElementById('conflicting_street_address').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(
      web_contents, click_conflicting_street_address_button_js));

  SubmitForm();
}

// Should be called for credit_card_upload_form_shipping_address.html.
void SaveCardBubbleViewsBrowserTestBase::
    FillAndSubmitFormWithConflictingPostalCode() {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_fill_button_js =
      "(function() { document.getElementById('fill_form').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

  const std::string click_conflicting_postal_code_button_js =
      "(function() { "
      "document.getElementById('conflicting_postal_code').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents,
                                     click_conflicting_postal_code_button_js));

  SubmitForm();
}

void SaveCardBubbleViewsBrowserTestBase::SetUploadDetailsRpcPaymentsAccepts() {
  test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                         kResponseGetUploadDetailsSuccess);
}

void SaveCardBubbleViewsBrowserTestBase::SetUploadDetailsRpcPaymentsDeclines() {
  test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                         kResponseGetUploadDetailsFailure);
}

void SaveCardBubbleViewsBrowserTestBase::SetUploadDetailsRpcServerError() {
  test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                         kResponseGetUploadDetailsSuccess,
                                         net::HTTP_INTERNAL_SERVER_ERROR);
}

void SaveCardBubbleViewsBrowserTestBase::SetUploadCardRpcPaymentsFails() {
  test_url_loader_factory()->AddResponse(kURLUploadCardRequest,
                                         kResponseGetUploadDetailsFailure);
}

void SaveCardBubbleViewsBrowserTestBase::ClickOnView(views::View* view) {
  DCHECK(view);
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMousePressed(pressed);
  ui::MouseEvent released_event = ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMouseReleased(released_event);
}

void SaveCardBubbleViewsBrowserTestBase::ClickOnDialogView(views::View* view) {
  GetSaveCardBubbleViews()
      ->GetDialogClientView()
      ->ResetViewShownTimeStampForTesting();
  views::BubbleFrameView* bubble_frame_view =
      static_cast<views::BubbleFrameView*>(GetSaveCardBubbleViews()
                                               ->GetWidget()
                                               ->non_client_view()
                                               ->frame_view());
  bubble_frame_view->ResetViewShownTimeStampForTesting();
  ClickOnView(view);
}

void SaveCardBubbleViewsBrowserTestBase::ClickOnDialogViewAndWait(
    views::View* view) {
  EXPECT_TRUE(GetSaveCardBubbleViews());
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetSaveCardBubbleViews()->GetWidget());
  ClickOnDialogView(view);
  destroyed_waiter.Wait();
  EXPECT_FALSE(GetSaveCardBubbleViews());
}

void SaveCardBubbleViewsBrowserTestBase::ClickOnDialogViewWithId(
    DialogViewId view_id) {
  ClickOnDialogView(FindViewInBubbleById(view_id));
}

void SaveCardBubbleViewsBrowserTestBase::ClickOnDialogViewWithIdAndWait(
    DialogViewId view_id) {
  ClickOnDialogViewAndWait(FindViewInBubbleById(view_id));
}

views::View* SaveCardBubbleViewsBrowserTestBase::FindViewInBubbleById(
    DialogViewId view_id) {
  SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
  DCHECK(save_card_bubble_views);

  views::View* specified_view =
      save_card_bubble_views->GetViewByID(static_cast<int>(view_id));

  if (!specified_view) {
    // Many of the save card bubble's inner Views are not child views but rather
    // contained by its DialogClientView. If we didn't find what we were looking
    // for, check there as well.
    specified_view = save_card_bubble_views->GetDialogClientView()->GetViewByID(
        static_cast<int>(view_id));
  }
  if (!specified_view) {
    // Additionally, the save card bubble's footnote view is not part of its
    // main bubble, and contains elements such as the legal message links.
    // If we didn't find what we were looking for, check there as well.
    views::View* footnote_view =
        save_card_bubble_views->GetFootnoteViewForTesting();
    if (footnote_view) {
      specified_view = footnote_view->GetViewByID(static_cast<int>(view_id));
    }
  }

  return specified_view;
}

void SaveCardBubbleViewsBrowserTestBase::ClickOnCancelButton() {
  SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
  DCHECK(save_card_bubble_views);
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_CLOSED});
  ClickOnDialogViewWithIdAndWait(DialogViewId::CANCEL_BUTTON);
  DCHECK(!GetSaveCardBubbleViews());
}

void SaveCardBubbleViewsBrowserTestBase::ClickOnCloseButton() {
  SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
  DCHECK(save_card_bubble_views);
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_CLOSED});
  ClickOnDialogViewAndWait(
      save_card_bubble_views->GetBubbleFrameView()->GetCloseButtonForTest());
  DCHECK(!GetSaveCardBubbleViews());
}

SaveCardBubbleViews*
SaveCardBubbleViewsBrowserTestBase::GetSaveCardBubbleViews() {
  SaveCardBubbleControllerImpl* save_card_bubble_controller_impl =
      SaveCardBubbleControllerImpl::FromWebContents(GetActiveWebContents());
  if (!save_card_bubble_controller_impl)
    return nullptr;
  SaveCardBubbleView* save_card_bubble_view =
      save_card_bubble_controller_impl->save_card_bubble_view();
  if (!save_card_bubble_view)
    return nullptr;
  return static_cast<SaveCardBubbleViews*>(save_card_bubble_view);
}

SaveCardIconView* SaveCardBubbleViewsBrowserTestBase::GetSaveCardIconView() {
  if (!browser())
    return nullptr;
  LocationBarView* location_bar_view =
      static_cast<LocationBarView*>(browser()->window()->GetLocationBar());
  DCHECK(location_bar_view->save_credit_card_icon_view());
  return location_bar_view->save_credit_card_icon_view();
}

content::WebContents*
SaveCardBubbleViewsBrowserTestBase::GetActiveWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void SaveCardBubbleViewsBrowserTestBase::AddEventObserverToController() {
  SaveCardBubbleControllerImpl* save_card_bubble_controller_impl =
      SaveCardBubbleControllerImpl::FromWebContents(GetActiveWebContents());
  DCHECK(save_card_bubble_controller_impl);
  save_card_bubble_controller_impl->SetEventObserverForTesting(this);
}

void SaveCardBubbleViewsBrowserTestBase::ReduceAnimationTime() {
  GetSaveCardIconView()->ReduceAnimationTimeForTesting();
}

void SaveCardBubbleViewsBrowserTestBase::ResetEventWaiterForSequence(
    std::list<DialogEvent> event_sequence) {
  event_waiter_ =
      std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
}

void SaveCardBubbleViewsBrowserTestBase::WaitForObservedEvent() {
  event_waiter_->Wait();
}

void SaveCardBubbleViewsBrowserTestBase::ReturnToInitialPage() {
  NavigateTo(test_file_path_);
}

network::TestURLLoaderFactory*
SaveCardBubbleViewsBrowserTestBase::test_url_loader_factory() {
  return &test_url_loader_factory_;
}

}  // namespace autofill
