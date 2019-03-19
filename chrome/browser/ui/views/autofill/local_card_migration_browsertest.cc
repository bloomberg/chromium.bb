// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctime>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
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
#include "chrome/browser/ui/views/autofill/migratable_card_view.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card_save_manager.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/window/dialog_client_view.h"

using base::Bucket;
using testing::ElementsAre;

namespace autofill {

namespace {

constexpr char kURLGetUploadDetailsRequest[] =
    "https://payments.google.com/payments/apis/chromepaymentsservice/"
    "getdetailsforsavecard";
constexpr char kURLMigrateCardRequest[] =
    "https://payments.google.com/payments/apis-secure/chromepaymentsservice/"
    "migratecards"
    "?s7e_suffix=chromewallet";

constexpr char kResponseGetUploadDetailsSuccess[] =
    "{\"legal_message\":{\"line\":[{\"template\":\"Legal message template with "
    "link: "
    "{0}.\",\"template_parameter\":[{\"display_text\":\"Link\",\"url\":\"https:"
    "//www.example.com/\"}]}]},\"context_token\":\"dummy_context_token\"}";
constexpr char kResponseGetUploadDetailsFailure[] =
    "{\"error\":{\"code\":\"FAILED_PRECONDITION\",\"user_error_message\":\"An "
    "unexpected error has occurred. Please try again later.\"}}";

constexpr char kResponseMigrateCardSuccess[] =
    "{\"save_result\":[{\"unique_id\":\"0\", \"status\":\"SUCCESS\"}, "
    "{\"unique_id\":\"1\", \"status\":\"SUCCESS\"}], "
    "\"value_prop_display_text\":\"example message.\"}";

constexpr char kCreditCardFormURL[] =
    "/credit_card_upload_form_address_and_cc.html";

constexpr char kFirstCardNumber[] = "5428424047572420";   // Mastercard
constexpr char kSecondCardNumber[] = "4782187095085933";  // Visa
constexpr char kThirdCardNumber[] = "4111111111111111";   // Visa
constexpr char kInvalidCardNumber[] = "4444444444444444";

constexpr double kFakeGeolocationLatitude = 1.23;
constexpr double kFakeGeolocationLongitude = 4.56;

}  // namespace

class LocalCardMigrationBrowserTest
    : public SyncTest,
      public LocalCardMigrationManager::ObserverForTest {
 protected:
  // Various events that can be waited on by the DialogEventWaiter.
  enum class DialogEvent : int {
    REQUESTED_LOCAL_CARD_MIGRATION,
    RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
    SENT_MIGRATE_CARDS_REQUEST,
    RECEIVED_MIGRATE_CARDS_RESPONSE
  };

  LocalCardMigrationBrowserTest() : SyncTest(SINGLE_CLIENT) {}

  ~LocalCardMigrationBrowserTest() override {}

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    // Set up the HTTPS server (uses the embedded_test_server).
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    embedded_test_server()->StartAcceptingConnections();

    ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(
        browser()->profile())
        ->OverrideNetworkResourcesForTest(
            std::make_unique<fake_server::FakeServerNetworkResources>(
                GetFakeServer()->AsWeakPtr()));

    std::string username;
#if defined(OS_CHROMEOS)
    // In ChromeOS browser tests, the profile may already by authenticated with
    // stub account |user_manager::kStubUserEmail|.
    CoreAccountInfo info =
        IdentityManagerFactory::GetForProfile(browser()->profile())
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
    local_card_migration_manager_ =
        ContentAutofillDriver::GetForRenderFrameHost(
            GetActiveWebContents()->GetMainFrame())
            ->autofill_manager()
            ->client()
            ->GetFormDataImporter()
            ->local_card_migration_manager_.get();

    local_card_migration_manager_->SetEventObserverForTesting(this);
    personal_data_ = local_card_migration_manager_->personal_data_manager_;

    // Set up the fake geolocation data.
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(
            kFakeGeolocationLatitude, kFakeGeolocationLongitude);

    // Set up billing customer ID.
    ContentAutofillDriver::GetForRenderFrameHost(
        GetActiveWebContents()->GetMainFrame())
        ->autofill_manager()
        ->client()
        ->GetPrefs()
        ->SetDouble(prefs::kAutofillBillingCustomerNumber, 1234);

    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillCreditCardLocalCardMigration);
    ASSERT_TRUE(harness_->SetupSync());
    SetUploadDetailsRpcPaymentsAccepts();
    SetUpMigrateCardsRpcPaymentsAccepts();
  }

  void NavigateTo(const std::string& file_path) {
    ui_test_utils::NavigateToURL(
        browser(), file_path.find("data:") == 0U
                       ? GURL(file_path)
                       : embedded_test_server()->GetURL(file_path));
  }

  void OnDecideToRequestLocalCardMigration() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::REQUESTED_LOCAL_CARD_MIGRATION);
  }

  void OnReceivedGetUploadDetailsResponse() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE);
  }

  void OnSentMigrateCardsRequest() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::SENT_MIGRATE_CARDS_REQUEST);
  }

  void OnReceivedMigrateCardsResponse() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::RECEIVED_MIGRATE_CARDS_RESPONSE);
  }

  CreditCard SaveLocalCard(std::string card_number,
                           bool set_as_expired_card = false) {
    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, "John Smith", card_number.c_str(),
                            "12",
                            set_as_expired_card ? test::LastYear().c_str()
                                                : test::NextYear().c_str(),
                            "1");
    local_card.set_guid("00000000-0000-0000-0000-" + card_number.substr(0, 12));
    local_card.set_record_type(CreditCard::LOCAL_CARD);
    AddTestCreditCard(browser(), local_card);
    return local_card;
  }

  CreditCard SaveServerCard(std::string card_number) {
    CreditCard server_card;
    test::SetCreditCardInfo(&server_card, "John Smith", card_number.c_str(),
                            "12", test::NextYear().c_str(), "1");
    server_card.set_guid("00000000-0000-0000-0000-" +
                         card_number.substr(0, 12));
    server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    server_card.set_server_id("full_id_" + card_number);
    AddTestServerCreditCard(browser(), server_card);
    return server_card;
  }

  void UseCardAndWaitForMigrationOffer(std::string card_number) {
    // Reusing a card should show the migration offer bubble.
    ResetEventWaiterForSequence(
        {DialogEvent::REQUESTED_LOCAL_CARD_MIGRATION,
         DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
    FillAndSubmitFormWithCard(card_number);
    WaitForObservedEvent();
  }

  void ClickOnSaveButtonAndWaitForMigrationResults() {
    ResetEventWaiterForSequence({DialogEvent::SENT_MIGRATE_CARDS_REQUEST,
                                 DialogEvent::RECEIVED_MIGRATE_CARDS_RESPONSE});
    ClickOnOkButton(GetLocalCardMigrationMainDialogView());
    WaitForObservedEvent();
  }

  void FillAndSubmitFormWithCard(std::string card_number) {
    NavigateTo(kCreditCardFormURL);
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

  void SetUploadDetailsRpcPaymentsAccepts() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponseGetUploadDetailsSuccess);
  }

  void SetUploadDetailsRpcPaymentsDeclines() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponseGetUploadDetailsFailure);
  }

  void SetUpMigrateCardsRpcPaymentsAccepts() {
    test_url_loader_factory()->AddResponse(kURLMigrateCardRequest,
                                           kResponseMigrateCardSuccess);
  }

  void ClickOnView(views::View* view) {
    DCHECK(view);
    ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMousePressed(pressed);
    ui::MouseEvent released_event =
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseReleased(released_event);
  }

  void ClickOnDialogViewAndWait(
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

  views::View* FindViewInDialogById(
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

  void ClickOnOkButton(views::DialogDelegateView* local_card_migration_view) {
    views::View* ok_button =
        local_card_migration_view->GetDialogClientView()->ok_button();

    ClickOnDialogViewAndWait(ok_button, local_card_migration_view);
  }

  void ClickOnCancelButton(
      views::DialogDelegateView* local_card_migration_view) {
    views::View* cancel_button =
        local_card_migration_view->GetDialogClientView()->cancel_button();
    ClickOnDialogViewAndWait(cancel_button, local_card_migration_view);
  }

  views::DialogDelegateView* GetLocalCardMigrationOfferBubbleViews() {
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

  views::DialogDelegateView* GetLocalCardMigrationMainDialogView() {
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

  LocalCardMigrationIconView* GetLocalCardMigrationIconView() {
    if (!browser())
      return nullptr;
    LocationBarView* location_bar_view =
        static_cast<LocationBarView*>(browser()->window()->GetLocationBar());
    DCHECK(location_bar_view->local_card_migration_icon_view());
    return location_bar_view->local_card_migration_icon_view();
  }

  views::View* GetCloseButton() {
    LocalCardMigrationBubbleViews* local_card_migration_bubble_views =
        static_cast<LocalCardMigrationBubbleViews*>(
            GetLocalCardMigrationOfferBubbleViews());
    DCHECK(local_card_migration_bubble_views);
    return local_card_migration_bubble_views->GetBubbleFrameView()
        ->GetCloseButtonForTest();
  }

  views::View* GetCardListView() {
    return static_cast<LocalCardMigrationDialogView*>(
               GetLocalCardMigrationMainDialogView())
        ->card_list_view_;
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence) {
    event_waiter_ =
        std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
  }

  void WaitForObservedEvent() { event_waiter_->Wait(); }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  void WaitForCardDeletion() { WaitForPersonalDataChange(browser()); }

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  LocalCardMigrationManager* local_card_migration_manager_;

  PersonalDataManager* personal_data_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<ProfileSyncServiceHarness> harness_;

 private:
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  std::unique_ptr<net::FakeURLFetcherFactory> url_fetcher_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationBrowserTest);
};

// Ensures that migration is not offered when user saves a new card.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       UsingNewCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  FillAndSubmitFormWithCard(kSecondCardNumber);

  // No migration bubble should be showing, because the single card upload
  // bubble should be displayed instead.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that migration is not offered when payments declines the cards.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    IntermediateMigrationOfferDoesNotShowWhenPaymentsDeclines) {
  base::HistogramTester histogram_tester;
  SetUploadDetailsRpcPaymentsDeclines();

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is not shown after reusing
// a saved server card, if there are no other cards to migrate.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReusingServerCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveServerCard(kFirstCardNumber);
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is shown after reusing
// a saved server card, if there is at least one card to migrate.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    ReusingServerCardWithMigratableLocalCardShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveServerCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);

  // The intermediate migration bubble should show.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->visible());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.FirstShow"),
      ElementsAre(
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationOrigin.UseOfServerCard",
      AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1);
}

// Ensures that the intermediate migration bubble is not shown after reusing
// a previously saved local card, if there are no other cards to migrate.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReusingLocalCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No migration bubble should be showing, because the single card upload
  // bubble should be displayed instead.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is triggered after reusing
// a saved local card, if there are multiple local cards available to migrate.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReusingLocalCardShowsIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);

  // The intermediate migration bubble should show.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->visible());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.FirstShow"),
      ElementsAre(
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationOrigin.UseOfLocalCard",
      AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1);
}

// Ensures that clicking [X] on the offer bubble makes the bubble disappear.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClickingCloseClosesBubble) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // Metrics
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationOrigin.UseOfLocalCard",
      AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1);
}

// Ensures that clicking on the credit card icon in the omnibox reopens the
// offer bubble after closing it.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClickingOmniboxIconReshowsBubble) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());
  ClickOnView(GetLocalCardMigrationIconView());

  // Clicking the icon should reshow the bubble.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->visible());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.Reshows"),
      ElementsAre(
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
}

// Ensures that accepting the intermediate migration offer opens up the main
// migration dialog.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClickingContinueOpensDialog) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());

  // Dialog should be visible.
  EXPECT_TRUE(FindViewInDialogById(
                  DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_OFFER_DIALOG,
                  GetLocalCardMigrationMainDialogView())
                  ->visible());
  // Intermediate bubble should be gone.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1),
                  Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_ACCEPTED, 1),
                  Bucket(AutofillMetrics::MAIN_DIALOG_SHOWN, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationBubbleUserInteraction.FirstShow",
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_ACCEPTED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationDialogOffer",
      AutofillMetrics::LOCAL_CARD_MIGRATION_DIALOG_SHOWN, 1);
}

// Ensures that the migration dialog contains all the valid card stored in
// Chrome browser local storage.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       DialogContainsAllValidMigratableCard) {
  base::HistogramTester histogram_tester;

  CreditCard first_card = SaveLocalCard(kFirstCardNumber);
  CreditCard second_card = SaveLocalCard(kSecondCardNumber);
  SaveLocalCard(kThirdCardNumber, /*set_as_expired_card=*/true);
  SaveLocalCard(kInvalidCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());

  views::View* card_list_view = GetCardListView();
  EXPECT_TRUE(card_list_view->visible());
  EXPECT_EQ(card_list_view->child_count(), 2);
  // Cards will be added to database in a reversed order.
  EXPECT_EQ(static_cast<MigratableCardView*>(card_list_view->child_at(0))
                ->GetNetworkAndLastFourDigits(),
            second_card.NetworkAndLastFourDigits());
  EXPECT_EQ(static_cast<MigratableCardView*>(card_list_view->child_at(1))
                ->GetNetworkAndLastFourDigits(),
            first_card.NetworkAndLastFourDigits());
}

// Ensures that rejecting the main migration dialog closes the dialog.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClickingCancelClosesDialog) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Cancel] button.
  ClickOnCancelButton(GetLocalCardMigrationMainDialogView());

  // No dialog should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationMainDialogView());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1),
                  Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_ACCEPTED, 1),
                  Bucket(AutofillMetrics::MAIN_DIALOG_SHOWN, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationDialogUserInteraction",
      AutofillMetrics::LOCAL_CARD_MIGRATION_DIALOG_CLOSED_CANCEL_BUTTON_CLICKED,
      1);
}

// Ensures that accepting the main migration dialog closes the dialog.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClickingSaveClosesDialog) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button in the bubble.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Save] button in the dialog.
  ClickOnSaveButtonAndWaitForMigrationResults();

  // No dialog should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationMainDialogView());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1),
                  Bucket(AutofillMetrics::INTERMEDIATE_BUBBLE_ACCEPTED, 1),
                  Bucket(AutofillMetrics::MAIN_DIALOG_SHOWN, 1),
                  Bucket(AutofillMetrics::MAIN_DIALOG_ACCEPTED, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.LocalCardMigrationDialogUserInteraction",
      AutofillMetrics::LOCAL_CARD_MIGRATION_DIALOG_CLOSED_SAVE_BUTTON_CLICKED,
      1);
}

// Ensures local cards will be deleted from browser local storage after being
// successfully migrated.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       DeleteSuccessfullyMigratedCardsFromLocal) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button in the bubble.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Save] button in the dialog.
  ClickOnSaveButtonAndWaitForMigrationResults();
  WaitForCardDeletion();

  EXPECT_EQ(nullptr, personal_data_->GetCreditCardByNumber(kFirstCardNumber));
  EXPECT_EQ(nullptr, personal_data_->GetCreditCardByNumber(kSecondCardNumber));
}

// Ensures that rejecting the main migration dialog adds 3 strikes.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClosingDialogAddsLocalCardMigrationStrikes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillLocalCardMigrationUsesStrikeSystemV2);
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Cancel] button, should add and log 3 strikes.
  ClickOnCancelButton(GetLocalCardMigrationMainDialogView());

  // Metrics
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.StrikeDatabase.NthStrikeAdded.LocalCardMigration"),
              ElementsAre(Bucket(3, 1)));
}

// Ensures that rejecting the migration bubble adds 2 strikes.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClosingBubbleAddsLocalCardMigrationStrikes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillLocalCardMigrationUsesStrikeSystemV2);
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // Metrics
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.StrikeDatabase.NthStrikeAdded.LocalCardMigration"),
              ElementsAre(Bucket(2, 1)));
}

// Ensures that reshowing and closing bubble after previously closing it does
// not add strikes.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReshowingBubbleDoesNotAddStrikes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillLocalCardMigrationUsesStrikeSystemV2);

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());
  base::HistogramTester histogram_tester;
  ClickOnView(GetLocalCardMigrationIconView());

  // Clicking the icon should reshow the bubble.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->visible());

  ClickOnDialogViewAndWait(GetCloseButton(),
                           GetLocalCardMigrationOfferBubbleViews());

  // Metrics
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// TODO(crbug.com/897998):
// - Update test set-up and add navagation tests.
// - Add more tests for feedback dialog.

}  // namespace autofill
