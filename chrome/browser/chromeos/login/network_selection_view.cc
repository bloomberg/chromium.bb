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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/widget/widget.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"

using views::Background;
using views::Label;
using views::View;
using views::Widget;

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
const SkColor kWelcomeColor = 0x0054A4;
const int kLanguageMainMenuSize = 5;

}  // namespace

namespace chromeos {

NetworkSelectionView::NetworkSelectionView(ScreenObserver* observer)
    : ALLOW_THIS_IN_INITIALIZER_LIST(menus::SimpleMenuModel(this)),
      network_combobox_(NULL),
      languages_menubutton_(NULL),
      welcome_label_(NULL),
      select_network_label_(NULL),
      connecting_network_label_(NULL),
      observer_(observer),
      network_notification_(false) {
  // TODO(glotov): need to specify the following list as a part of the
  // image customization.
  languages_model_.CopySpecifiedLanguagesUp("es,it,de,fr,en-US");
}

NetworkSelectionView::~NetworkSelectionView() {
  if (network_notification_)
    ChangeNetworkNotification(false);
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

  network_combobox_ = new views::Combobox(this);
  network_combobox_->set_listener(this);

  languages_menubutton_ = new views::MenuButton(
      NULL, std::wstring(), this, true);
  InitLanguageMenu();

  offline_button_ = new views::NativeButton(this, std::wstring());
  offline_button_->set_font(button_font);

  UpdateLocalizedStrings();

  AddChildView(welcome_label_);
  AddChildView(select_network_label_);
  AddChildView(connecting_network_label_);
  AddChildView(network_combobox_);
  AddChildView(languages_menubutton_);
  AddChildView(offline_button_);
}

void NetworkSelectionView::InitLanguageMenu() {
  int line = 0;
  while (line != kLanguageMainMenuSize) {
    AddItem(line, WideToUTF16(languages_model_.GetLanguageNameAt(line)));
    line++;
  }
  AddSeparator();
  languages_submenu_.reset(new menus::SimpleMenuModel(this));
  AddSubMenu(WideToUTF16(l10n_util::GetString(IDS_LANGUAGES_MORE)),
             languages_submenu_.get());
  while (line != languages_model_.get_languages_count()) {
    languages_submenu_->AddItem(
        line, WideToUTF16(languages_model_.GetLanguageNameAt(line)));
    line++;
  }
}

void NetworkSelectionView::UpdateLocalizedStrings() {
  const std::string locale = g_browser_process->GetApplicationLocale();
  const std::wstring locale_name = languages_model_.GetLanguageNameAt(
      languages_model_.GetIndexFromLocale(locale));
  languages_menubutton_->SetText(locale_name);
  welcome_label_->SetText(l10n_util::GetStringF(IDS_NETWORK_SELECTION_TITLE,
                          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
  select_network_label_->SetText(
      l10n_util::GetString(IDS_NETWORK_SELECTION_SELECT));
  offline_button_->SetLabel(
      l10n_util::GetString(IDS_NETWORK_SELECTION_OFFLINE_BUTTON));
  UpdateConnectingNetworkLabel();
}

void NetworkSelectionView::Refresh() {
  ChangeNetworkNotification(true);
  NetworkChanged(chromeos::NetworkLibrary::Get());
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
// ComboboxModel implementation:

int NetworkSelectionView::GetItemCount() {
  // Item with index = 0 is either "no networks are available" or
  // "no selection".
  return static_cast<int>(networks_.GetNetworkCount()) + 1;
}

std::wstring NetworkSelectionView::GetItemAt(int index) {
  if (index == 0) {
    return networks_.IsEmpty() ?
        l10n_util::GetString(IDS_STATUSBAR_NO_NETWORKS_MESSAGE) :
        l10n_util::GetString(IDS_NETWORK_SELECTION_NONE);
  }
  NetworkList::NetworkItem* network =
      networks_.GetNetworkAt(index - 1);
  return network ? UTF16ToWide(network->label) : std::wstring();
}

////////////////////////////////////////////////////////////////////////////////
// views::Combobox::Listener implementation:

void NetworkSelectionView::ItemChanged(views::Combobox* sender,
                                       int prev_index,
                                       int new_index) {
  if (new_index == prev_index || new_index < 0 || prev_index < 0)
    return;

  // First item is a text, not a network.
  if (new_index == 0) {
    network_combobox_->SetSelectedItem(prev_index);
    return;
  }

  if (networks_.IsEmpty())
    return;

  NetworkList::NetworkItem* network =
      networks_.GetNetworkAt(new_index - 1);
  if (network) {
    if (NetworkList::NETWORK_WIFI == network->network_type) {
      if (network->wifi_network.encrypted) {
        OpenPasswordDialog(network->wifi_network);
        return;
      } else {
        NetworkLibrary::Get()->ConnectToWifiNetwork(
            network->wifi_network, string16());
      }
    } else if (NetworkList::NETWORK_CELLULAR ==
               network->network_type) {
      NetworkLibrary::Get()->ConnectToCellularNetwork(
          network->cellular_network);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation:

void NetworkSelectionView::ButtonPressed(views::Button* sender,
                                         const views::Event& event) {
  if (observer_) {
    ChangeNetworkNotification(false);
    observer_->OnExit(ScreenObserver::NETWORK_OFFLINE);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary::Observer implementation:

void NetworkSelectionView::NetworkChanged(
    NetworkLibrary* network_lib) {
  // Save network selection in case it would be available after refresh.
  NetworkList::NetworkType network_type = NetworkList::NETWORK_EMPTY;
  string16 network_id;
  const NetworkList::NetworkItem* network = GetSelectedNetwork();
  if (network) {
    network_type = network->network_type;
    network_id = network->label;
  }
  networks_.NetworkChanged(network_lib);
  // TODO(nkostylev): Check for connection error.
  if (networks_.ConnectedNetwork()) {
    NotifyOnConnection();
  }
  network = networks_.ConnectingNetwork();
  if (network) {
    ShowConnectingStatus(true, network->label);
  }
  network_combobox_->ModelChanged();
  SelectNetwork(network_type, network_id);
}

void NetworkSelectionView::NetworkTraffic(NetworkLibrary* cros,
                                          int traffic_type) {
}

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation.

void NetworkSelectionView::RunMenu(View* source, const gfx::Point& pt) {
  if (languages_menu_ == NULL) {
    languages_menu_.reset(new views::Menu2(this));
  }
  languages_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// menus::SimpleMenuModel::Delegate implementation.

bool NetworkSelectionView::IsCommandIdChecked(int command_id) const {
  return false;
}

bool NetworkSelectionView::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool NetworkSelectionView::GetAcceleratorForCommandId(
    int command_id, menus::Accelerator* accelerator) {
  return false;
}

void NetworkSelectionView::ExecuteCommand(int command_id) {
  const std::string locale = languages_model_.GetLocaleFromIndex(command_id);
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kApplicationLocale, UTF8ToWide(locale));
  prefs->ScheduleSavePersistentPrefs();
  observer_->OnSwitchLanguage(locale);
  // Don't do anything here because |this| has just been deleted in order
  // to force releasing all locale-specific data.
}

////////////////////////////////////////////////////////////////////////////////
// NetworkSelectionView, private:

NetworkList::NetworkItem* NetworkSelectionView::GetSelectedNetwork() {
  return networks_.GetNetworkAt(network_combobox_->selected_item() - 1);
}

void NetworkSelectionView::NotifyOnConnection() {
  if (observer_) {
    ChangeNetworkNotification(false);
    observer_->OnExit(ScreenObserver::NETWORK_CONNECTED);
  }
}

void NetworkSelectionView::OpenPasswordDialog(WifiNetwork network) {
  NetworkConfigView* view = new NetworkConfigView(network, true);
  views::Window* window = views::Window::CreateChromeWindow(
      GetWindow()->GetNativeWindow(), gfx::Rect(), view);
  window->SetIsAlwaysOnTop(true);
  window->Show();
  view->SetLoginTextfieldFocus();
}

void NetworkSelectionView::SelectNetwork(
    NetworkList::NetworkType type, const string16& id) {
  int index = networks_.GetNetworkIndexById(type, id);
  if (index >= 0) {
    network_combobox_->SetSelectedItem(index + 1);
  } else {
    network_combobox_->SetSelectedItem(0);
  }
}

void NetworkSelectionView::ShowConnectingStatus(bool connecting,
                                                const string16& network_id) {
  network_id_ = network_id;
  UpdateConnectingNetworkLabel();
  select_network_label_->SetVisible(!connecting);
  network_combobox_->SetVisible(!connecting);
  connecting_network_label_->SetVisible(connecting);
}

void NetworkSelectionView::ChangeNetworkNotification(bool subscribe) {
  network_notification_ = subscribe;
  if (subscribe)
    chromeos::NetworkLibrary::Get()->AddObserver(this);
  else
    chromeos::NetworkLibrary::Get()->RemoveObserver(this);
}

void NetworkSelectionView::UpdateConnectingNetworkLabel() {
  connecting_network_label_->SetText(l10n_util::GetStringF(
      IDS_NETWORK_SELECTION_CONNECTING, UTF16ToWide(network_id_)));
}

}  // namespace chromeos
