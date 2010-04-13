// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/network_selection_view.h"

#include <signal.h>
#include <sys/types.h>
#include <string>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/network_screen_delegate.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/language_switch_model.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/widget/widget.h"
#include "views/widget/widget_gtk.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"

using views::Background;
using views::Label;
using views::View;
using views::Widget;
using views::WidgetGtk;

namespace {

const int kWelcomeLabelY = 150;
const int kOfflineButtonX = 30;
const int kSpacing = 25;
const int kComboboxSpacing = 5;
const int kHorizontalSpacing = 25;
const int kNetworkComboboxWidth = 250;
const int kNetworkComboboxHeight = 30;
const int kLanguagesMenuWidth = 200;
const int kLanguagesMenuHeight = 30;
const SkColor kWelcomeColor = 0xFF1D6AB1;

}  // namespace

namespace chromeos {

NetworkSelectionView::NetworkSelectionView(NetworkScreenDelegate* delegate)
    : network_combobox_(NULL),
      languages_menubutton_(NULL),
      welcome_label_(NULL),
      select_network_label_(NULL),
      connecting_network_label_(NULL),
      delegate_(delegate) {
}

NetworkSelectionView::~NetworkSelectionView() {
}

void NetworkSelectionView::Init() {
  // Use rounded rect background.
  views::Painter* painter = CreateWizardPainter(
      &BorderDefinition::kScreenBorder);
  set_background(
      views::Background::CreateBackgroundPainter(true, painter));

  gfx::Font welcome_label_font =
      gfx::Font::CreateFont(L"Droid Sans", 20).DeriveFont(0, gfx::Font::BOLD);
  gfx::Font label_font = gfx::Font::CreateFont(L"Droid Sans", 9);
  gfx::Font button_font = label_font;

  welcome_label_ = new views::Label();
  welcome_label_->SetColor(kWelcomeColor);
  welcome_label_->SetFont(welcome_label_font);

  select_network_label_ = new views::Label();
  select_network_label_->SetFont(label_font);

  connecting_network_label_ = new views::Label();
  connecting_network_label_->SetFont(label_font);
  connecting_network_label_->SetVisible(false);

  network_combobox_ = new views::Combobox(delegate_);
  network_combobox_->set_listener(delegate_);

  languages_menubutton_ = new views::MenuButton(
      NULL, std::wstring(), delegate_->language_switch_model(), true);

  offline_button_ = new views::NativeButton(delegate_, std::wstring());
  offline_button_->set_font(button_font);

  UpdateLocalizedStrings();

  AddChildView(welcome_label_);
  AddChildView(select_network_label_);
  AddChildView(connecting_network_label_);
  AddChildView(network_combobox_);
  AddChildView(languages_menubutton_);
  AddChildView(offline_button_);
}

void NetworkSelectionView::UpdateLocalizedStrings() {
  languages_menubutton_->SetText(
      delegate_->language_switch_model()->GetCurrentLocaleName());
  welcome_label_->SetText(l10n_util::GetStringF(IDS_NETWORK_SELECTION_TITLE,
                          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
  select_network_label_->SetText(
      l10n_util::GetString(IDS_NETWORK_SELECTION_SELECT));
  offline_button_->SetLabel(
      l10n_util::GetString(IDS_NETWORK_SELECTION_OFFLINE_BUTTON));
  UpdateConnectingNetworkLabel();
}

////////////////////////////////////////////////////////////////////////////////
// views::View: implementation:

gfx::Size NetworkSelectionView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

void NetworkSelectionView::Layout() {
  int y = kWelcomeLabelY;
  welcome_label_->SetBounds(
      (width() - welcome_label_->GetPreferredSize().width()) / 2,
      y,
      welcome_label_->GetPreferredSize().width(),
      welcome_label_->GetPreferredSize().height());

  int select_network_x = (width() -
      select_network_label_->GetPreferredSize().width() -
      kNetworkComboboxWidth) / 2;
  y += welcome_label_->GetPreferredSize().height() + kSpacing;
  select_network_label_->SetBounds(
      select_network_x,
      y,
      select_network_label_->GetPreferredSize().width(),
      select_network_label_->GetPreferredSize().height());
  connecting_network_label_->SetBounds(
      kHorizontalSpacing,
      y,
      width() - kHorizontalSpacing * 2,
      connecting_network_label_->GetPreferredSize().height());

  select_network_x += select_network_label_->GetPreferredSize().width() +
      kHorizontalSpacing;
  y -= kComboboxSpacing;
  network_combobox_->SetBounds(select_network_x, y,
                               kNetworkComboboxWidth, kNetworkComboboxHeight);

  y = height() - offline_button_->GetPreferredSize().height() - kSpacing;
  offline_button_->SetBounds(
      kOfflineButtonX,
      y,
      offline_button_->GetPreferredSize().width(),
      offline_button_->GetPreferredSize().height());

  int x = width() - kLanguagesMenuWidth - kHorizontalSpacing;
  y = kSpacing;
  languages_menubutton_->SetBounds(x, y,
                                   kLanguagesMenuWidth, kLanguagesMenuHeight);

  // Need to refresh combobox layout explicitly.
  network_combobox_->Layout();
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkSelectionView, public:

int NetworkSelectionView::GetSelectedNetworkItem() const {
  return network_combobox_->selected_item();
}

void NetworkSelectionView::SetSelectedNetworkItem(int index) {
  network_combobox_->SetSelectedItem(index);
}

gfx::NativeWindow NetworkSelectionView::GetNativeWindow() {
  return GTK_WINDOW(static_cast<WidgetGtk*>(GetWidget())->GetNativeView());
}

void NetworkSelectionView::NetworkModelChanged() {
  network_combobox_->ModelChanged();
}

void NetworkSelectionView::ShowConnectingStatus(bool connecting,
                                                const string16& network_id) {
  network_id_ = network_id;
  UpdateConnectingNetworkLabel();
  select_network_label_->SetVisible(!connecting);
  network_combobox_->SetVisible(!connecting);
  connecting_network_label_->SetVisible(connecting);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkSelectionView, private:

void NetworkSelectionView::UpdateConnectingNetworkLabel() {
  connecting_network_label_->SetText(l10n_util::GetStringF(
      IDS_NETWORK_SELECTION_CONNECTING, UTF16ToWide(network_id_)));
}

}  // namespace chromeos
