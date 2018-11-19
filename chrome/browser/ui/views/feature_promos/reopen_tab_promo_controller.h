// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_REOPEN_TAB_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_REOPEN_TAB_PROMO_CONTROLLER_H_

#include "chrome/browser/ui/views/toolbar/app_menu_observer.h"
#include "ui/views/controls/menu/menu_listener.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;
class FeaturePromoBubbleView;
class ReopenTabInProductHelp;

// Handles display of the reopen tab in-product help promo, including showing
// the promo bubble and highlighting the appropriate app menu items. Notifies
// the |ReopenTabInProductHelp| service when the promo is finished.
class ReopenTabPromoController : public AppMenuObserver,
                                 public views::MenuListener,
                                 public views::WidgetObserver {
 public:
  explicit ReopenTabPromoController(BrowserView* browser_view);
  ~ReopenTabPromoController() override = default;

  void ShowPromo();

 private:
  // views::MenuListener:
  void OnMenuOpened() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // AppMenuObserver:
  void AppMenuClosed() override;
  void OnShowSubmenu() override;

  ReopenTabInProductHelp* const iph_service_;
  BrowserView* const browser_view_;
  FeaturePromoBubbleView* promo_bubble_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_REOPEN_TAB_PROMO_CONTROLLER_H_
