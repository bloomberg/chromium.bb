// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_browsertest_base.h"
#include "content/public/test/browser_test_utils.h"

using base::Bucket;
using testing::ElementsAre;

namespace {
constexpr char kFirstCardNumber[] = "5428424047572420";   // Mastercard
constexpr char kSecondCardNumber[] = "4782187095085933";  // Visa
constexpr char kThirdCardNumber[] = "345159891979146";    // AmEx
}  // namespace

namespace autofill {

class LocalCardMigrationBrowserTest : public LocalCardMigrationBrowserTestBase {
 protected:
  LocalCardMigrationBrowserTest()
      : LocalCardMigrationBrowserTestBase(
            "/credit_card_upload_form_address_and_cc.html") {}

  // Returns the number of migratable cards.
  void TriggerMigrationBubbleByReusingLocalCard() {
    SaveLocalCard(kFirstCardNumber);
    SaveLocalCard(kSecondCardNumber);
    SignInWithFullName("John Smith");
    UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  }

  void TriggerMigrationBubbleByReusingServerCard() {
    SaveLocalCard(kFirstCardNumber);
    SaveLocalCard(kSecondCardNumber);
    SignInWithFullName("John Smith");
    SaveServerCard(kThirdCardNumber);
    UseCardAndWaitForMigrationOffer(kThirdCardNumber);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationBrowserTest);
};

// Ensures that migration is not offered when user saves a new card.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       UsingNewCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;
  SetUploadDetailsRpcPaymentsAccepts();

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  SignInWithFullName("John Smith");

  FillAndSubmitFormWithCard(kThirdCardNumber);

  // No bubble should be showing.
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

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  SignInWithFullName("John Smith");

  SetUploadDetailsRpcPaymentsDeclines();
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());

  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is not shown after reusing a
// saved server card, if there are no other cards to migrate.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReusingServerCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;
  SetUploadDetailsRpcPaymentsAccepts();

  SignInWithFullName("John Smith");
  SaveServerCard(kFirstCardNumber);
  SaveServerCard(kSecondCardNumber);
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());

  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is not shown after reusing a
// previously saved local card, if there are no other cards to migrate.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReusingLocalCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;
  SetUploadDetailsRpcPaymentsAccepts();

  SaveLocalCard(kFirstCardNumber);
  SignInWithFullName("John Smith");
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No migration bubble should be showing, because the single card upload
  // bubble should be displayed instead.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());

  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is triggered after reusing a
// saved server card, if there are migratable cards available.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReusingServerCardShowsIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;
  TriggerMigrationBubbleByReusingServerCard();

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

// Ensures that the intermediate migration bubble is triggered after reusing a
// saved local card, if there are multiple local cards available to migrate.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ReusingLocalCardShowsIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;
  TriggerMigrationBubbleByReusingLocalCard();

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
  TriggerMigrationBubbleByReusingLocalCard();

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
  TriggerMigrationBubbleByReusingLocalCard();

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
  TriggerMigrationBubbleByReusingLocalCard();

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

// Ensures that rejecting the main migration dialog closes the dialog.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest,
                       ClickingCancelClosesDialog) {
  base::HistogramTester histogram_tester;
  TriggerMigrationBubbleByReusingLocalCard();

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
  TriggerMigrationBubbleByReusingLocalCard();

  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());

  // Click the [Save] button and wait for RPC to go through.
  ResetEventWaiterForSequence({DialogEvent::SENT_MIGRATE_LOCAL_CARDS_REQUEST,
                               DialogEvent::RECEIVED_MIGRATE_CARDS_RESPONSE});
  ClickOnOkButton(GetLocalCardMigrationMainDialogView());
  WaitForObservedEvent();

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

// Ensures that all cards are visible in the main dialog.
IN_PROC_BROWSER_TEST_F(LocalCardMigrationBrowserTest, AllCardsVisibleOnDialog) {
  base::HistogramTester histogram_tester;
  TriggerMigrationBubbleByReusingLocalCard();

  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());

  views::View* card_list_view = GetCardListView();
  EXPECT_TRUE(card_list_view->visible());

  // TriggerMigrationBubbleByReusingLocalCard() saves two cards.
  EXPECT_TRUE(card_list_view->child_at(0)->visible());
  EXPECT_TRUE(card_list_view->child_at(1)->visible());

  // Click the [Save] button and wait for RPC to go through.
  ResetEventWaiterForSequence({DialogEvent::SENT_MIGRATE_LOCAL_CARDS_REQUEST,
                               DialogEvent::RECEIVED_MIGRATE_CARDS_RESPONSE});
  ClickOnOkButton(GetLocalCardMigrationMainDialogView());
  WaitForObservedEvent();
}

// TODO(manasverma): Add these following tests.
// 1) Local cards should get deleted after migration.
// 2) Expired and invalid cards should not be shown.
// 3) When user navigates away after five seconds, the bubble disappears.
// 4) When user navigates away after five seconds, the dialog should stay.
// 5) When user clicks legal message links, browser should redirect to different
//    tab but dialog should stay in the original tab.
// 6) Simulate check boxes to ensure
//    LocalCardMigrationDialogUserSelectionPercentage is logged correctly.
// 7) Ensure LocalCardMigrationDialogActiveDuration is logged correctly.

}  // namespace autofill
