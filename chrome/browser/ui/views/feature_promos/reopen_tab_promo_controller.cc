// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/reopen_tab_promo_controller.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/in_product_help/in_product_help.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help_factory.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace {

constexpr char kReopenTabPromoDismissedAtHistogram[] =
    "InProductHelp.Promos.IPH_ReopenTab.DismissedAt";

}  // namespace

ReopenTabPromoController::ReopenTabPromoController(BrowserView* browser_view)
    : iph_service_(ReopenTabInProductHelpFactory::GetForProfile(
          browser_view->browser()->profile())),
      browser_view_(browser_view) {
}

void ReopenTabPromoController::ShowPromo() {
  // This shouldn't be called more than once. Check that state is fresh.
  DCHECK_EQ(StepAtDismissal::kBubbleShown, promo_step_);

  // Here, we start the promo display. We highlight the app menu button and open
  // the promo bubble.
  auto* app_menu_button = browser_view_->toolbar()->app_menu_button();
  app_menu_button->AddObserver(this);
  app_menu_button->SetPromoFeature(InProductHelpFeature::kReopenTab);

  promo_bubble_ = FeaturePromoBubbleView::CreateOwned(
      app_menu_button, views::BubbleBorder::Arrow::TOP_RIGHT,
      IDS_REOPEN_TAB_PROMO, FeaturePromoBubbleView::ActivationAction::ACTIVATE);
  promo_bubble_->set_close_on_deactivate(false);
  promo_bubble_->GetWidget()->AddObserver(this);
}

void ReopenTabPromoController::OnTabReopened(int command_id) {
  iph_service_->TabReopened();

  if (command_id == AppMenuModel::kMinRecentTabsCommandId) {
    DCHECK_EQ(StepAtDismissal::kMenuOpened, promo_step_);
    promo_step_ = StepAtDismissal::kTabReopened;
  }
}

void ReopenTabPromoController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(promo_bubble_);
  promo_bubble_ = nullptr;

  // If the menu isn't showing, that means the promo bubble timed out.
  if (!browser_view_->toolbar()->app_menu_button()->IsMenuShowing())
    PromoEnded();
}

void ReopenTabPromoController::AppMenuShown() {
  // Close the promo bubble since it doesn't automatically close on click.
  promo_bubble_->GetWidget()->Close();

  // Stop showing promo on app menu button.
  browser_view_->toolbar()->app_menu_button()->SetPromoFeature(base::nullopt);

  promo_step_ = StepAtDismissal::kMenuOpened;
}

void ReopenTabPromoController::AppMenuClosed() {
  PromoEnded();
}

void ReopenTabPromoController::PromoEnded() {
  UMA_HISTOGRAM_ENUMERATION(kReopenTabPromoDismissedAtHistogram, promo_step_);

  // We notify the service regardless of whether IPH succeeded. Success is
  // determined by whether the reopen tab event was sent.
  iph_service_->HelpDismissed();

  auto* app_menu_button = browser_view_->toolbar()->app_menu_button();
  app_menu_button->SetPromoFeature(base::nullopt);
  app_menu_button->RemoveObserver(this);
}
