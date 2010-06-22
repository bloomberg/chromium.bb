// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/network_selection_view.h"

#include <signal.h>
#include <sys/types.h>
#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/network_screen_delegate.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
#include "views/widget/widget.h"
#include "views/widget/widget_gtk.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"

using views::Background;
using views::Label;
using views::SmoothedThrobber;
using views::View;
using views::Widget;
using views::WidgetGtk;

namespace {

const int kWelcomeLabelY = 170;
const int kContinueButtonSpacingX = 30;
const int kSpacing = 25;
const int kHorizontalSpacing = 25;
const int kSelectionBoxWidthMin = 200;
const int kSelectionBoxHeight = 29;
const int kSelectionBoxSpacing = 7;

// Menu button is drawn using our custom icons in resources. See
// TextButtonBorder::Paint() for details. So this offset compensate
// horizontal size, eaten by those icons.
const int kMenuButtonHorizontalOffset = 1;

// Vertical addition to the menu window to make it appear exactly below
// MenuButton.
const int kMenuButtonVerticalOffset = 3;

const SkColor kWelcomeColor = 0xFF1D6AB1;

const int kThrobberFrameMs = 60;
const int kThrobberStartDelayMs = 500;

}  // namespace

namespace chromeos {

NetworkSelectionView::NetworkSelectionView(NetworkScreenDelegate* delegate)
    : network_combobox_(NULL),
      languages_menubutton_(NULL),
      welcome_label_(NULL),
      select_language_label_(NULL),
      select_network_label_(NULL),
      connecting_network_label_(NULL),
      continue_button_(NULL),
      throbber_(NULL),
      delegate_(delegate) {
}

NetworkSelectionView::~NetworkSelectionView() {
  network_combobox_->set_listener(NULL);
  network_combobox_ = NULL;
  throbber_->Stop();
  throbber_ = NULL;
}

void NetworkSelectionView::Init() {
  // Use rounded rect background.
  views::Painter* painter = CreateWizardPainter(
      &BorderDefinition::kScreenBorder);
  set_background(
      views::Background::CreateBackgroundPainter(true, painter));

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font welcome_label_font =
      rb.GetFont(ResourceBundle::LargeFont).DeriveFont(0, gfx::Font::BOLD);

  welcome_label_ = new views::Label();
  welcome_label_->SetColor(kWelcomeColor);
  welcome_label_->SetFont(welcome_label_font);
  welcome_label_->SetMultiLine(true);

  select_language_label_ = new views::Label();
  select_language_label_->SetFont(rb.GetFont(ResourceBundle::MediumFont));

  select_network_label_ = new views::Label();
  select_network_label_->SetFont(rb.GetFont(ResourceBundle::MediumFont));

  connecting_network_label_ = new views::Label();
  connecting_network_label_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  connecting_network_label_->SetVisible(false);

  throbber_ = new views::SmoothedThrobber(kThrobberFrameMs);
  throbber_->SetFrames(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_SPINNER));
  throbber_->set_start_delay_ms(kThrobberStartDelayMs);
  AddChildView(throbber_);

  network_combobox_ = new views::Combobox(delegate_);
  network_combobox_->set_listener(delegate_);

  languages_menubutton_ = new views::MenuButton(
      NULL, std::wstring(), delegate_->language_switch_menu(), true);
  languages_menubutton_->SetFocusable(true);
  languages_menubutton_->SetNormalHasBorder(true);
  delegate_->language_switch_menu()->set_menu_offset(
      kMenuButtonHorizontalOffset, kMenuButtonVerticalOffset);

  AddChildView(welcome_label_);
  AddChildView(select_language_label_);
  AddChildView(select_network_label_);
  AddChildView(connecting_network_label_);
  AddChildView(languages_menubutton_);
  AddChildView(network_combobox_);

  UpdateLocalizedStrings();
}

void NetworkSelectionView::UpdateLocalizedStrings() {
  RecreateNativeControls();
  languages_menubutton_->SetText(
      delegate_->language_switch_menu()->GetCurrentLocaleName());
  welcome_label_->SetText(l10n_util::GetStringF(IDS_NETWORK_SELECTION_TITLE,
                          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
  select_language_label_->SetText(
      l10n_util::GetString(IDS_LANGUAGE_SELECTION_SELECT));
  select_network_label_->SetText(
      l10n_util::GetString(IDS_NETWORK_SELECTION_SELECT));
  continue_button_->SetLabel(
      l10n_util::GetString(IDS_NETWORK_SELECTION_CONTINUE_BUTTON));
  UpdateConnectingNetworkLabel();
}

////////////////////////////////////////////////////////////////////////////////
// views::View: implementation:

void NetworkSelectionView::ChildPreferredSizeChanged(View* child) {
  Layout();
  SchedulePaint();
}

void NetworkSelectionView::LocaleChanged() {
  UpdateLocalizedStrings();
  NetworkModelChanged();
  // Explicitly set selected item - index 0 is a localized string.
  if (GetSelectedNetworkItem() <= 0 &&
      delegate_->GetItemCount() > 0) {
    SetSelectedNetworkItem(0);
  }
  Layout();
  SchedulePaint();
}

gfx::Size NetworkSelectionView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

void NetworkSelectionView::Layout() {
  gfx::Insets insets = GetInsets();
  int max_width = this->width() - insets.width() - 2 * kHorizontalSpacing;
  welcome_label_->SizeToFit(max_width);
  int y = kWelcomeLabelY;
  y -= welcome_label_->GetPreferredSize().height() / 2;

  welcome_label_->SetBounds(
      (width() - welcome_label_->GetPreferredSize().width()) / 2,
      y,
      welcome_label_->GetPreferredSize().width(),
      welcome_label_->GetPreferredSize().height());
  y += welcome_label_->GetPreferredSize().height() + kSpacing;

  // Use menu preffered size to calculate boxes width accordingly.
  int box_width = delegate_->language_switch_menu()->GetFirstLevelMenuWidth() +
      kMenuButtonHorizontalOffset * 2;
  const int widest_label = std::max(
      select_language_label_->GetPreferredSize().width(),
      select_network_label_->GetPreferredSize().width());
  if (box_width < kSelectionBoxWidthMin) {
    box_width = kSelectionBoxWidthMin;
    delegate_->language_switch_menu()->SetFirstLevelMenuWidth(
        box_width - kMenuButtonHorizontalOffset * 2);
  } else if (widest_label + box_width + 2 * kHorizontalSpacing > width()) {
    box_width = width() - widest_label - 2 * kHorizontalSpacing;
  }
  const int labels_x = (width() - widest_label - box_width) / 2;
  select_language_label_->SetBounds(
      labels_x,
      y,
      select_language_label_->GetPreferredSize().width(),
      select_language_label_->GetPreferredSize().height());

  const int selection_box_x = labels_x + widest_label + kHorizontalSpacing;
  const int label_y_offset =
      (kSelectionBoxHeight -
       select_language_label_->GetPreferredSize().height()) / 2;
  languages_menubutton_->SetBounds(selection_box_x, y - label_y_offset,
                                   box_width, kSelectionBoxHeight);

  y += kSelectionBoxHeight + kSelectionBoxSpacing;
  select_network_label_->SetBounds(
      labels_x,
      y,
      select_network_label_->GetPreferredSize().width(),
      select_network_label_->GetPreferredSize().height());

  connecting_network_label_->SetBounds(
      kHorizontalSpacing,
      y,
      width() - kHorizontalSpacing * 2,
      connecting_network_label_->GetPreferredSize().height());

  throbber_->SetBounds(
      width() / 2 + connecting_network_label_->GetPreferredSize().width() / 2 +
          kHorizontalSpacing,
      y + (connecting_network_label_->GetPreferredSize().height() -
           throbber_->GetPreferredSize().height()) / 2,
      throbber_->GetPreferredSize().width(),
      throbber_->GetPreferredSize().height());

  network_combobox_->SetBounds(selection_box_x, y - label_y_offset,
                               box_width, kSelectionBoxHeight);

  y = height() - continue_button_->GetPreferredSize().height() - kSpacing;
  continue_button_->SetBounds(
      width() - kContinueButtonSpacingX -
          continue_button_->GetPreferredSize().width(),
      y,
      continue_button_->GetPreferredSize().width(),
      continue_button_->GetPreferredSize().height());

  // Need to refresh combobox layout explicitly.
  network_combobox_->Layout();
  continue_button_->Layout();
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
  select_language_label_->SetVisible(!connecting);
  languages_menubutton_->SetVisible(!connecting);
  select_network_label_->SetVisible(!connecting);
  network_combobox_->SetVisible(!connecting);
  continue_button_->SetVisible(!connecting);
  connecting_network_label_->SetVisible(connecting);
  Layout();
  if (connecting) {
    throbber_->Start();
  } else {
    throbber_->Stop();
  }
}

void NetworkSelectionView::EnableContinue(bool enabled) {
  if (continue_button_)
    continue_button_->SetEnabled(enabled);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkSelectionView, private:

void NetworkSelectionView::RecreateNativeControls() {
  // There is no way to get native button preferred size after the button was
  // sized so delete and recreate the button on text update.
  delete continue_button_;
  continue_button_ = new views::NativeButton(delegate_, std::wstring());
  continue_button_->SetEnabled(false);
  AddChildView(continue_button_);
}

void NetworkSelectionView::UpdateConnectingNetworkLabel() {
  connecting_network_label_->SetText(l10n_util::GetStringF(
      IDS_NETWORK_SELECTION_CONNECTING, UTF16ToWide(network_id_)));
}

}  // namespace chromeos
