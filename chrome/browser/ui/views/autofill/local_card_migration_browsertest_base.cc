// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/local_card_migration_browsertest_base.h"

#include <ctime>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_account_fetcher_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/local_card_migration_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_dialog_view.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/credit_card_save_manager.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/test_sync_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

namespace {
constexpr char kURLGetUploadDetailsRequest[] =
    "https://payments.google.com/payments/apis/chromepaymentsservice/"
    "getdetailsforsavecard";
constexpr char kResponseGetUploadDetailsSuccess[] =
    "{\"legal_message\":{\"line\":[{\"template\":\"Legal message template with "
    "link: "
    "{0}.\",\"template_parameter\":[{\"display_text\":\"Link\",\"url\":\"https:"
    "//www.example.com/\"}]}]},\"context_token\":\"dummy_context_token\"}";
constexpr char kResponseGetUploadDetailsFailure[] =
    "{\"error\":{\"code\":\"FAILED_PRECONDITION\",\"user_error_message\":\"An "
    "unexpected error has occurred. Please try again later.\"}}";

constexpr double kFakeGeolocationLatitude = 1.23;
constexpr double kFakeGeolocationLongitude = 4.56;
}  // namespace

LocalCardMigrationBrowserTestBase::LocalCardMigrationBrowserTestBase(
    const std::string& test_file_path)
    : test_file_path_(test_file_path) {}

LocalCardMigrationBrowserTestBase::~LocalCardMigrationBrowserTestBase() {}

void LocalCardMigrationBrowserTestBase::SetUpOnMainThread() {
  // Enable Local Card Migration.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillCreditCardLocalCardMigration);

  // Set up the HTTPS server (uses the embedded_test_server).
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "components/test/data/autofill");
  embedded_test_server()->StartAcceptingConnections();

  // Set up the URL loader factory for the payments client so we can intercept
  // those network requests too.
  test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  ContentAutofillDriver::GetForRenderFrameHost(
      GetActiveWebContents()->GetMainFrame())
      ->autofill_manager()
      ->payments_client()
      ->set_url_loader_factory_for_testing(test_shared_loader_factory_);

  // Set up this class as the ObserverForTest implementation.
  LocalCardMigrationManager* local_card_migration_manager =
      ContentAutofillDriver::GetForRenderFrameHost(
          GetActiveWebContents()->GetMainFrame())
          ->autofill_manager()
          ->form_data_importer_.get()
          ->local_card_migration_manager();

  local_card_migration_manager->SetEventObserverForTesting(this);
  personal_data_ = local_card_migration_manager->personal_data_manager_;

  // Set up the fake geolocation data.
  geolocation_overrider_ = std::make_unique<device::ScopedGeolocationOverrider>(
      kFakeGeolocationLatitude, kFakeGeolocationLongitude);

  ContentAutofillDriver::GetForRenderFrameHost(
      GetActiveWebContents()->GetMainFrame())
      ->autofill_manager()
      ->client()
      ->GetPrefs()
      ->SetDouble(prefs::kAutofillBillingCustomerNumber, 1234);

  NavigateTo(test_file_path_);
}

void LocalCardMigrationBrowserTestBase::NavigateTo(
    const std::string& file_path) {
  ui_test_utils::NavigateToURL(browser(),
                               file_path.find("data:") == 0U
                                   ? GURL(file_path)
                                   : embedded_test_server()->GetURL(file_path));
}

void LocalCardMigrationBrowserTestBase::OnSentMigrateLocalCardsRequest() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::SENT_MIGRATE_LOCAL_CARDS_REQUEST);
}

void LocalCardMigrationBrowserTestBase::OnReceivedGetUploadDetailsResponse() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE);
}

void LocalCardMigrationBrowserTestBase::OnDecideToRequestLocalCardMigration() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::REQUESTED_LOCAL_CARD_MIGRATION);
}

void LocalCardMigrationBrowserTestBase::OnRecievedMigrateCardsResponse() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::RECEIVED_MIGRATE_CARDS_RESPONSE);
}

void LocalCardMigrationBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  will_create_browser_context_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
              base::BindRepeating(&LocalCardMigrationBrowserTestBase::
                                      OnWillCreateBrowserContextServices,
                                  base::Unretained(this)));
}

void LocalCardMigrationBrowserTestBase::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  // Replace the signin manager and account fetcher service with fakes.
  SigninManagerFactory::GetInstance()->SetTestingFactory(
      context, &BuildFakeSigninManagerForTesting);
  AccountFetcherServiceFactory::GetInstance()->SetTestingFactory(
      context, &FakeAccountFetcherServiceBuilder::BuildForTests);
}

void LocalCardMigrationBrowserTestBase::SaveLocalCard(std::string card_number) {
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "John Smith", card_number.c_str(), "12",
                          test::NextYear().c_str(), "1");
  local_card.set_guid("00000000-0000-0000-0000-" + card_number.substr(0, 12));
  local_card.set_record_type(CreditCard::LOCAL_CARD);
  personal_data_->AddCreditCard(local_card);
}

void LocalCardMigrationBrowserTestBase::SaveServerCard(
    std::string card_number) {
  CreditCard server_card;
  test::SetCreditCardInfo(&server_card, "John Smith", card_number.c_str(), "12",
                          test::NextYear().c_str(), "1");
  server_card.set_guid("00000000-0000-0000-0000-" + card_number.substr(0, 12));
  server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
  server_card.set_server_id("full_id_" + card_number);
  personal_data_->AddFullServerCreditCard(server_card);
}

void LocalCardMigrationBrowserTestBase::UseCardAndWaitForMigrationOffer(
    std::string card_number) {
  NavigateTo("/credit_card_upload_form_address_and_cc.html");

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Once signed-in, reusing a card should show the migration offer bubble.
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_LOCAL_CARD_MIGRATION,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillAndSubmitFormWithCard(card_number);
  WaitForObservedEvent();
}

void LocalCardMigrationBrowserTestBase::SignInWithFullName(
    const std::string& full_name) {
  // TODO(crbug.com/859761): Can this function be used to remove the
  // observer_for_testing_ hack in
  // CreditCardSaveManager::IsCreditCardUploadEnabled()?
  FakeSigninManagerForTesting* signin_manager =
      static_cast<FakeSigninManagerForTesting*>(
          SigninManagerFactory::GetInstance()->GetForProfile(
              browser()->profile()));

  // Note: Chrome OS tests seem to rely on these specific login values, so
  //       changing them is probably not recommended.
  constexpr char kTestEmail[] = "stub-user@example.com";
  constexpr char kTestGaiaId[] = "stub-user@example.com";
#if !defined(OS_CHROMEOS)
  signin_manager->SignIn(kTestGaiaId, kTestEmail, "password");
#else
  AccountTrackerService* account_tracker_service =
      AccountTrackerServiceFactory::GetForProfile(browser()->profile());
  signin_manager->SignIn(account_tracker_service->PickAccountIdForAccount(
      kTestGaiaId, kTestEmail));
#endif
  FakeAccountFetcherService* account_fetcher_service =
      static_cast<FakeAccountFetcherService*>(
          AccountFetcherServiceFactory::GetForProfile(browser()->profile()));
  account_fetcher_service->FakeUserInfoFetchSuccess(
      signin_manager->GetAuthenticatedAccountId(), kTestEmail, kTestGaiaId,
      AccountTrackerService::kNoHostedDomainFound, full_name,
      /*given_name=*/std::string(), "locale", "avatar.jpg");
}

// Should be called for credit_card_upload_form_address_and_cc.html.
void LocalCardMigrationBrowserTestBase::FillAndSubmitFormWithCard(
    std::string card_number) {
  content::WebContents* web_contents = GetActiveWebContents();

  const std::string click_fill_button_js =
      "(function() { document.getElementById('fill_form').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

  const std::string fill_cc_number_js =
      "(function() { document.getElementsByName(\"cc_number\")[0].value = " +
      card_number + "; })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, fill_cc_number_js));

  const std::string click_submit_button_js =
      "(function() { document.getElementById('submit').click(); })();";
  content::TestNavigationObserver nav_observer(web_contents);
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_submit_button_js));
  nav_observer.Wait();
}

void LocalCardMigrationBrowserTestBase::SetUploadDetailsRpcPaymentsAccepts() {
  test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                         kResponseGetUploadDetailsSuccess);
}

void LocalCardMigrationBrowserTestBase::SetUploadDetailsRpcPaymentsDeclines() {
  test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                         kResponseGetUploadDetailsFailure);
}

void LocalCardMigrationBrowserTestBase::ClickOnView(views::View* view) {
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

void LocalCardMigrationBrowserTestBase::ClickOnDialogViewAndWait(
    views::View* view,
    views::DialogDelegateView* local_card_migration_view) {
  DCHECK(local_card_migration_view);
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      local_card_migration_view->GetWidget());
  local_card_migration_view->GetDialogClientView()
      ->ResetViewShownTimeStampForTesting();
  views::BubbleFrameView* bubble_frame_view =
      static_cast<views::BubbleFrameView*>(
          local_card_migration_view->GetWidget()
              ->non_client_view()
              ->frame_view());
  bubble_frame_view->ResetViewShownTimeStampForTesting();
  ClickOnView(view);
  destroyed_waiter.Wait();
}

views::View* LocalCardMigrationBrowserTestBase::FindViewInDialogById(
    DialogViewId view_id,
    views::DialogDelegateView* local_card_migration_view) {
  DCHECK(local_card_migration_view);

  views::View* specified_view =
      local_card_migration_view->GetViewByID(static_cast<int>(view_id));

  if (!specified_view) {
    specified_view =
        local_card_migration_view->GetDialogClientView()->GetViewByID(
            static_cast<int>(view_id));
  }

  return specified_view;
}

void LocalCardMigrationBrowserTestBase::ClickOnOkButton(
    views::DialogDelegateView* local_card_migration_view) {
  views::View* ok_button =
      local_card_migration_view->GetDialogClientView()->ok_button();

  ClickOnDialogViewAndWait(ok_button, local_card_migration_view);
}

void LocalCardMigrationBrowserTestBase::ClickOnCancelButton(
    views::DialogDelegateView* local_card_migration_view) {
  views::View* cancel_button =
      local_card_migration_view->GetDialogClientView()->cancel_button();
  ClickOnDialogViewAndWait(cancel_button, local_card_migration_view);
}

views::DialogDelegateView*
LocalCardMigrationBrowserTestBase::GetLocalCardMigrationOfferBubbleViews() {
  LocalCardMigrationBubbleControllerImpl*
      local_card_migration_bubble_controller_impl =
          LocalCardMigrationBubbleControllerImpl::FromWebContents(
              GetActiveWebContents());
  if (!local_card_migration_bubble_controller_impl)
    return nullptr;
  return static_cast<LocalCardMigrationBubbleViews*>(
      local_card_migration_bubble_controller_impl
          ->local_card_migration_bubble_view());
}

views::DialogDelegateView*
LocalCardMigrationBrowserTestBase::GetLocalCardMigrationMainDialogView() {
  LocalCardMigrationDialogControllerImpl*
      local_card_migration_dialog_controller_impl =
          LocalCardMigrationDialogControllerImpl::FromWebContents(
              GetActiveWebContents());
  if (!local_card_migration_dialog_controller_impl)
    return nullptr;
  return static_cast<LocalCardMigrationDialogView*>(
      local_card_migration_dialog_controller_impl
          ->local_card_migration_dialog_view());
}

LocalCardMigrationIconView*
LocalCardMigrationBrowserTestBase::GetLocalCardMigrationIconView() {
  if (!browser())
    return nullptr;
  LocationBarView* location_bar_view =
      static_cast<LocationBarView*>(browser()->window()->GetLocationBar());
  DCHECK(location_bar_view->local_card_migration_icon_view());
  return location_bar_view->local_card_migration_icon_view();
}

views::View* LocalCardMigrationBrowserTestBase::GetCloseButton() {
  LocalCardMigrationBubbleViews* local_card_migration_bubble_views =
      static_cast<LocalCardMigrationBubbleViews*>(
          GetLocalCardMigrationOfferBubbleViews());
  DCHECK(local_card_migration_bubble_views);
  return local_card_migration_bubble_views->GetBubbleFrameView()
      ->GetCloseButtonForTest();
}

views::View* LocalCardMigrationBrowserTestBase::GetCardListView() {
  return static_cast<LocalCardMigrationDialogView*>(
             GetLocalCardMigrationMainDialogView())
      ->card_list_view_;
}

content::WebContents*
LocalCardMigrationBrowserTestBase::GetActiveWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void LocalCardMigrationBrowserTestBase::ResetEventWaiterForSequence(
    std::list<DialogEvent> event_sequence) {
  event_waiter_ =
      std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
}

void LocalCardMigrationBrowserTestBase::WaitForObservedEvent() {
  event_waiter_->Wait();
}

network::TestURLLoaderFactory*
LocalCardMigrationBrowserTestBase::test_url_loader_factory() {
  return &test_url_loader_factory_;
}

}  // namespace autofill
