// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_BROWSERTEST_BASE_H_

#include <list>
#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_dialog_view.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_icon_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card_save_manager.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {
class ScopedGeolocationOverrider;
}

namespace autofill {

// Base class for any interactive LocalCardMigrationBubbleViews and
// LocalCardMigrationDialogView browser tests that will need to show and
// interact with the local card migration flow.
class LocalCardMigrationBrowserTestBase
    : public InProcessBrowserTest,
      public LocalCardMigrationManager::ObserverForTest {
 protected:
  // Various events that can be waited on by the DialogEventWaiter.
  enum class DialogEvent : int {
    REQUESTED_LOCAL_CARD_MIGRATION,
    RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
    SENT_MIGRATE_LOCAL_CARDS_REQUEST,
    RECEIVED_MIGRATE_CARDS_RESPONSE
  };

  // Test will open a browser window to |test_file_path| (relative to
  // components/test/data/autofill).
  explicit LocalCardMigrationBrowserTestBase(const std::string& test_file_path);
  ~LocalCardMigrationBrowserTestBase() override;

  void SetUpOnMainThread() override;

  void NavigateTo(const std::string& file_path);

  // LocalCardMigrationManager::ObserverForTest:
  void OnDecideToRequestLocalCardMigration() override;
  void OnReceivedGetUploadDetailsResponse() override;
  void OnSentMigrateLocalCardsRequest() override;
  void OnRecievedMigrateCardsResponse() override;

  // BrowserTestBase:
  void SetUpInProcessBrowserTestFixture() override;

  // Sets up the ability to sign in the user.
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);

  // Saves a local card.
  void SaveLocalCard(std::string card_number);

  // Saves a full server card.
  void SaveServerCard(std::string card_number);

  // Submits a checkout form with given card and waits for a migration offer.
  void UseCardAndWaitForMigrationOffer(std::string card_number);

  // Signs in the user with the provided |full_name|.
  void SignInWithFullName(const std::string& full_name);

  // Will call JavaScript to fill and submit the form.
  void FillAndSubmitFormWithCard(std::string card_number);

  // For setting up Payments RPCs.
  void SetUploadDetailsRpcPaymentsAccepts();
  void SetUploadDetailsRpcPaymentsDeclines();

  // Clicks on the given views::View*.
  void ClickOnView(views::View* view);

  // Clicks on the given views::View* and resets the associated
  // DialogDelegateView.
  void ClickOnDialogViewAndWait(
      views::View* view,
      views::DialogDelegateView* local_card_migration_view);

  // Returns the views::View* that was previously assigned the id |view_id|.
  views::View* FindViewInDialogById(
      DialogViewId view_id,
      views::DialogDelegateView* local_card_migration_view);

  // Clicks on dialog button in |local_card_migration_view|.
  void ClickOnOkButton(views::DialogDelegateView* local_card_migration_view);
  void ClickOnCancelButton(
      views::DialogDelegateView* local_card_migration_view);

  // Gets the views::DialogDelegateView* instance of the intermediate migration
  // offer bubble.
  views::DialogDelegateView* GetLocalCardMigrationOfferBubbleViews();

  // Gets the views::DialogDelegateView* instance of the main migration dialog
  // view.
  views::DialogDelegateView* GetLocalCardMigrationMainDialogView();

  // Gets the credit card icon in the omnibox.
  LocalCardMigrationIconView* GetLocalCardMigrationIconView();

  views::View* GetCloseButton();

  views::View* GetCardListView();

  content::WebContents* GetActiveWebContents();

  // Resets the event waiter for a given |event_sequence|.
  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence);

  // Wait for any events passed through ResetEventWaiterForSequence() to occur.
  void WaitForObservedEvent();

  network::TestURLLoaderFactory* test_url_loader_factory();

  PersonalDataManager* personal_data_ = nullptr;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  std::unique_ptr<net::FakeURLFetcherFactory> url_fetcher_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
  const std::string test_file_path_;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationBrowserTestBase);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_BROWSERTEST_BASE_H_
