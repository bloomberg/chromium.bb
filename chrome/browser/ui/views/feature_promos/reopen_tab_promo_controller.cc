// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/reopen_tab_promo_controller.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help_factory.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace {

// Last step of the flow completed by the user before dismissal (whether by
// successful completion of the flow, timing out, or clicking away.). This is
// used for an UMA histogram.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ReopenTabPromoStepAtDismissal {
  // The promo bubble was shown, but the menu was not opened; i.e. the bubble
  // timed out.
  kBubbleShown = 0,
  // The menu was opened, but the user clicked away without opening the last
  // closed tab.
  kMenuOpened = 1,
  // The last closed tab item was clicked. The promo was successful.
  kTabReopened = 2,

  kMaxValue = kTabReopened,
};

const char kReopenTabPromoDismissedAtHistogram[] =
    "InProductHelp.Promos.IPH_ReopenTab.DismissedAt";

}  // namespace

ReopenTabPromoController::ReopenTabPromoController(BrowserView* browser_view)
    : iph_service_(ReopenTabInProductHelpFactory::GetForProfile(
          browser_view->browser()->profile())),
      browser_view_(browser_view) {
  // Check that the app menu button exists. It should only not exist when there
  // is no tab strip, in which case this shouldn't trigger in the first place.
  BrowserAppMenuButton* app_menu_button =
      browser_view_->toolbar()->app_menu_button();
  DCHECK(app_menu_button);
}

void ReopenTabPromoController::ShowPromo() {
  // This shouldn't be called more than once. Check that state is fresh.
  DCHECK(!tab_reopened_before_app_menu_closed_);

  // Here, we start the promo display. We highlight the app menu button and open
  // the promo bubble.
  BrowserAppMenuButton* app_menu_button =
      browser_view_->toolbar()->app_menu_button();
  app_menu_button->AddMenuListener(this);
  app_menu_button->SetPromoIsShowing(true);

  promo_bubble_ = FeaturePromoBubbleView::CreateOwned(
      app_menu_button, views::BubbleBorder::Arrow::TOP_RIGHT,
      IDS_REOPEN_TAB_PROMO, FeaturePromoBubbleView::ActivationAction::ACTIVATE);
  promo_bubble_->set_close_on_deactivate(false);
  promo_bubble_->GetWidget()->AddObserver(this);
}

void ReopenTabPromoController::OnMenuOpened() {
  // The user followed the promo and opened the menu. First, we close the promo
  // bubble since it doesn't automatically close on click. Then, we highlight
  // the history item and observe for the history submenu opening.
  promo_bubble_->GetWidget()->Close();

  BrowserAppMenuButton* app_menu_button =
      browser_view_->toolbar()->app_menu_button();
  app_menu_button->RemoveMenuListener(this);

  AppMenu* app_menu = app_menu_button->app_menu();
  app_menu->AddObserver(this);
  app_menu->ShowReopenTabPromo();
}

void ReopenTabPromoController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(promo_bubble_);
  promo_bubble_ = nullptr;

  // If the menu isn't showing, that means the promo bubble timed out. We should
  // notify our IPH service that help was dismissed.
  if (!browser_view_->toolbar()->app_menu_button()->IsMenuShowing()) {
    UMA_HISTOGRAM_ENUMERATION(kReopenTabPromoDismissedAtHistogram,
                              ReopenTabPromoStepAtDismissal::kBubbleShown);

    BrowserAppMenuButton* app_menu_button =
        browser_view_->toolbar()->app_menu_button();
    app_menu_button->RemoveMenuListener(this);
    app_menu_button->SetPromoIsShowing(false);
    iph_service_->HelpDismissed();
  }
}

void ReopenTabPromoController::AppMenuClosed() {
  // The menu was opened then closed, whether by clicking away or by clicking a
  // menu item. We notify the service regardless of whether IPH succeeded.
  // Success is determined by whether the reopen tab event was sent.
  if (!tab_reopened_before_app_menu_closed_) {
    UMA_HISTOGRAM_ENUMERATION(kReopenTabPromoDismissedAtHistogram,
                              ReopenTabPromoStepAtDismissal::kMenuOpened);
  }

  iph_service_->HelpDismissed();

  browser_view_->toolbar()->app_menu_button()->SetPromoIsShowing(false);

  AppMenu* app_menu = browser_view_->toolbar()->app_menu_button()->app_menu();
  app_menu->RemoveObserver(this);
}

void ReopenTabPromoController::OnExecuteCommand(int command_id) {
  if (command_id == AppMenuModel::kMinRecentTabsCommandId) {
    DCHECK(!tab_reopened_before_app_menu_closed_);
    UMA_HISTOGRAM_ENUMERATION(kReopenTabPromoDismissedAtHistogram,
                              ReopenTabPromoStepAtDismissal::kTabReopened);
    tab_reopened_before_app_menu_closed_ = true;
  }
}
