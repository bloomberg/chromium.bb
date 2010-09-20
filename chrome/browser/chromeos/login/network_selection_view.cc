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
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/network_screen_delegate.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/status/network_dropdown_button.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
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

const int kWelcomeLabelY = 70;
const int kContinueButtonSpacingX = 30;
const int kSpacing = 25;
const int kHorizontalSpacing = 25;
const int kSelectionBoxWidthMin = 200;
const int kSelectionBoxHeight = 29;
const int kSelectionBoxSpacing = 7;

// Menu button is drawn using our custom icons in resources. See
// TextButtonBorder::Paint() for details. So this offset compensate
// horizontal size, eaten by those icons.
const int kMenuHorizontalOffset = -1;

// Vertical addition to the menu window to make it appear exactly below
// MenuButton.
const int kMenuVerticalOffset = 3;

// Offset that compensates menu width so that it matches
// menu button visual width when being in pushed state.
const int kMenuWidthOffset = 6;

const SkColor kWelcomeColor = 0xFF1D6AB1;

}  // namespace

namespace chromeos {

// NetworkDropdownButton with custom accelerator enabled.
class NetworkControlWithAccelerators : public NetworkDropdownButton {
 public:
  NetworkControlWithAccelerators(bool browser_mode,
                                 gfx::NativeWindow parent_window,
                                 NetworkScreenDelegate* delegate)
      : NetworkDropdownButton(browser_mode, parent_window),
        delegate_(delegate),
        accel_clear_errors_(views::Accelerator(app::VKEY_ESCAPE,
                                               false, false, false)) {
    AddAccelerator(accel_clear_errors_);
  }

  // Overridden from View:
  bool AcceleratorPressed(const views::Accelerator& accel) {
    if (accel == accel_clear_errors_) {
      delegate_->ClearErrors();
      return true;
    }
    return false;
  }

  // Overridden from MenuButton:
  virtual bool Activate() {
    delegate_->ClearErrors();
    return MenuButton::Activate();
  }

 private:
  NetworkScreenDelegate* delegate_;

  // ESC accelerator for closing error info bubble.
  views::Accelerator accel_clear_errors_;

  DISALLOW_COPY_AND_ASSIGN(NetworkControlWithAccelerators);
};

// MenuButton with custom processing on focus events.
class NotifyingMenuButton : public views::MenuButton {
 public:
  NotifyingMenuButton(views::ButtonListener* listener,
                      const std::wstring& text,
                      views::ViewMenuDelegate* menu_delegate,
                      bool show_menu_marker,
                      NetworkScreenDelegate* delegate)
      : MenuButton(listener, text, menu_delegate, show_menu_marker),
        delegate_(delegate) {}

  // Overridden from View:
  virtual void DidGainFocus() {
    delegate_->ClearErrors();
  }

 private:
  NetworkScreenDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NotifyingMenuButton);
};

NetworkSelectionView::NetworkSelectionView(NetworkScreenDelegate* delegate)
    : languages_menubutton_(NULL),
      welcome_label_(NULL),
      select_language_label_(NULL),
      select_network_label_(NULL),
      connecting_network_label_(NULL),
      network_dropdown_(NULL),
      continue_button_(NULL),
      throbber_(CreateDefaultSmoothedThrobber()),
      proxy_settings_link_(NULL),
      continue_button_order_index_(-1),
      delegate_(delegate) {
}

NetworkSelectionView::~NetworkSelectionView() {
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

  languages_menubutton_ = new NotifyingMenuButton(
      NULL, std::wstring(), delegate_->language_switch_menu(), true, delegate_);
  languages_menubutton_->SetFocusable(true);
  languages_menubutton_->SetNormalHasBorder(true);
  // Menu is positioned by bottom right corner of the MenuButton.
  delegate_->language_switch_menu()->set_menu_offset(kMenuHorizontalOffset,
                                                     kMenuVerticalOffset);

  network_dropdown_ = new NetworkControlWithAccelerators(false,
                                                         GetNativeWindow(),
                                                         delegate_);
  network_dropdown_->SetNormalHasBorder(true);
  network_dropdown_->SetFocusable(true);

  proxy_settings_link_ = new views::Link();
  proxy_settings_link_->SetController(this);
  proxy_settings_link_->SetVisible(true);
  proxy_settings_link_->SetFocusable(true);

  AddChildView(welcome_label_);
  AddChildView(select_language_label_);
  AddChildView(select_network_label_);
  AddChildView(connecting_network_label_);
  AddChildView(throbber_);
  AddChildView(languages_menubutton_);
  AddChildView(network_dropdown_);
  AddChildView(proxy_settings_link_);

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
  proxy_settings_link_->SetText(
      l10n_util::GetString(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));
  UpdateConnectingNetworkLabel();
}

////////////////////////////////////////////////////////////////////////////////
// views::View: implementation:

void NetworkSelectionView::ChildPreferredSizeChanged(View* child) {
  Layout();
  SchedulePaint();
}

void NetworkSelectionView::OnLocaleChanged() {
  UpdateLocalizedStrings();
  // Proxy settings dialog contains localized title.  Zap it.
  proxy_settings_dialog_.reset(NULL);

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
      kMenuWidthOffset;
  const int widest_label = std::max(
      select_language_label_->GetPreferredSize().width(),
      select_network_label_->GetPreferredSize().width());
  if (box_width < kSelectionBoxWidthMin) {
    box_width = kSelectionBoxWidthMin;
    delegate_->language_switch_menu()->SetFirstLevelMenuWidth(
        box_width - kMenuWidthOffset);
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

  network_dropdown_->SetBounds(selection_box_x, y - label_y_offset,
                               box_width, kSelectionBoxHeight);

  y += kSelectionBoxHeight + kSelectionBoxSpacing;
  proxy_settings_link_->SetBounds(
      selection_box_x,
      y,
      std::min(box_width, proxy_settings_link_->GetPreferredSize().width()),
      proxy_settings_link_->GetPreferredSize().height());

  y = height() - continue_button_->GetPreferredSize().height() - kSpacing;
  continue_button_->SetBounds(
      width() - kContinueButtonSpacingX -
          continue_button_->GetPreferredSize().width(),
      y,
      continue_button_->GetPreferredSize().width(),
      continue_button_->GetPreferredSize().height());

  // Need to refresh combobox layout explicitly.
  continue_button_->Layout();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkSelectionView, public:

gfx::NativeWindow NetworkSelectionView::GetNativeWindow() const {
  return GTK_WINDOW(static_cast<WidgetGtk*>(GetWidget())->GetNativeView());
}

views::View* NetworkSelectionView::GetNetworkControlView() const {
  return network_dropdown_;
}

void NetworkSelectionView::ShowConnectingStatus(bool connecting,
                                                const string16& network_id) {
  network_id_ = network_id;
  UpdateConnectingNetworkLabel();
  select_language_label_->SetVisible(!connecting);
  languages_menubutton_->SetVisible(!connecting);
  select_network_label_->SetVisible(!connecting);
  network_dropdown_->SetVisible(!connecting);
  continue_button_->SetVisible(!connecting);
  connecting_network_label_->SetVisible(connecting);
  Layout();
  if (connecting) {
    throbber_->Start();
    network_dropdown_->CancelMenu();
  } else {
    throbber_->Stop();
  }
}

bool NetworkSelectionView::IsConnecting() const {
  return connecting_network_label_->IsVisible();
}

void NetworkSelectionView::EnableContinue(bool enabled) {
  if (continue_button_)
    continue_button_->SetEnabled(enabled);
}

bool NetworkSelectionView::IsContinueEnabled() const {
  return continue_button_ && continue_button_->IsEnabled();
}

////////////////////////////////////////////////////////////////////////////////
// views::LinkController implementation:

void NetworkSelectionView::LinkActivated(views::Link* source, int) {
  if (source == proxy_settings_link_) {
    if (!proxy_settings_dialog_.get()) {
      static const char kProxySettingsURL[] = "chrome://options/proxy";
      proxy_settings_dialog_.reset(new LoginHtmlDialog(
          this,
          GetNativeWindow(),
          l10n_util::GetString(IDS_OPTIONS_PROXY_TAB_LABEL),
          GURL(kProxySettingsURL)));
    }
    proxy_settings_dialog_->Show();
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkSelectionView, private:

void NetworkSelectionView::RecreateNativeControls() {
  // There is no way to get native button preferred size after the button was
  // sized so delete and recreate the button on text update.
  bool is_continue_enabled = IsContinueEnabled();
  delete continue_button_;
  continue_button_ = new views::NativeButton(
      delegate_,
      l10n_util::GetString(IDS_NETWORK_SELECTION_CONTINUE_BUTTON));
  continue_button_->SetEnabled(is_continue_enabled);
  if (continue_button_order_index_ < 0) {
    continue_button_order_index_ = GetChildViewCount();
  }
  AddChildView(continue_button_order_index_, continue_button_);
}

void NetworkSelectionView::UpdateConnectingNetworkLabel() {
  connecting_network_label_->SetText(l10n_util::GetStringF(
      IDS_NETWORK_SELECTION_CONNECTING, UTF16ToWide(network_id_)));
}

}  // namespace chromeos
