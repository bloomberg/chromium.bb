// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/incognito_menu_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

IncognitoMenuView::IncognitoMenuView(views::Button* anchor_button,
                                     const gfx::Rect& anchor_rect,
                                     gfx::NativeView parent_window,
                                     Browser* browser)
    : ProfileMenuViewBase(anchor_button, anchor_rect, parent_window, browser) {
  DCHECK(browser->profile()->IsIncognito());

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::INCOGNITO_WINDOW_COUNT);

  base::RecordAction(base::UserMetricsAction("IncognitoMenu_Show"));
}

IncognitoMenuView::~IncognitoMenuView() = default;

void IncognitoMenuView::Reset() {
  ProfileMenuViewBase::Reset();
  title_card_ = nullptr;
  close_button_ = nullptr;
}

void IncognitoMenuView::Init() {
  Reset();
  AddIncognitoWindowCountView();
  RepopulateViewFromMenuItems();
}

base::string16 IncognitoMenuView::GetAccessibleWindowTitle() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_INCOGNITO_BUBBLE_ACCESSIBLE_TITLE,
      BrowserList::GetIncognitoSessionsActiveForProfile(browser()->profile()));
}

void IncognitoMenuView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  DCHECK_EQ(sender, close_button_);

  // Skipping before-unload trigger to give incognito mode users a chance to
  // quickly close all incognito windows without needing to confirm closing the
  // open forms.
  BrowserList::CloseAllBrowsersWithIncognitoProfile(
      browser()->profile(), base::DoNothing(), base::DoNothing(),
      true /* skip_beforeunload */);
}

void IncognitoMenuView::AddIncognitoWindowCountView() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  int incognito_window_count =
      BrowserList::GetIncognitoSessionsActiveForProfile(browser()->profile());
  // The icon color is set to match the menu text, which guarantees sufficient
  // contrast and a consistent visual appearance.
  const SkColor icon_color = provider->GetTypographyProvider().GetColor(
      *this, views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);

  auto incognito_icon = std::make_unique<views::ImageView>();
  incognito_icon->SetImage(
      gfx::CreateVectorIcon(kIncognitoProfileIcon, icon_color));

  // TODO(https://crbug.com/915120): This Button is never clickable. Replace
  // by an alternative list item.
  std::unique_ptr<HoverButton> title_card = std::make_unique<HoverButton>(
      nullptr, std::move(incognito_icon),
      l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_TITLE),
      incognito_window_count > 1
          ? l10n_util::GetPluralStringFUTF16(IDS_INCOGNITO_WINDOW_COUNT_MESSAGE,
                                             incognito_window_count)
          : base::string16());
  title_card->SetEnabled(false);
  title_card_ = title_card.get();

  ProfileMenuViewBase::MenuItems menu_items;
  menu_items.push_back(std::move(title_card));
  AddMenuItems(menu_items, true);

  std::unique_ptr<HoverButton> close_button = std::make_unique<HoverButton>(
      this, gfx::CreateVectorIcon(kCloseAllIcon, 16, gfx::kChromeIconGrey),
      l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_CLOSE_BUTTON));
  close_button_ = close_button.get();

  menu_items.clear();
  menu_items.push_back(std::move(close_button));
  AddMenuItems(menu_items, true);
}
