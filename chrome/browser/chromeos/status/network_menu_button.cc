// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_button.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "views/window/window.h"

namespace {

// Time in milliseconds to delay showing of promo
// notification when Chrome window is not on screen.
const int kPromoShowDelayMs = 10000;

const int kNotificationCountPrefDefault = -1;

bool GetBooleanPref(const char* pref_name) {
  Browser* browser = BrowserList::GetLastActive();
  // Default to safe value which is false (not to show bubble).
  if (!browser || !browser->profile())
    return false;

  PrefService* prefs = browser->profile()->GetPrefs();
  return prefs->GetBoolean(pref_name);
}

int GetIntegerPref(const char* pref_name) {
  Browser* browser = BrowserList::GetLastActive();
  // Default to "safe" value.
  if (!browser || !browser->profile())
    return kNotificationCountPrefDefault;

  PrefService* prefs = browser->profile()->GetPrefs();
  return prefs->GetInteger(pref_name);
}

void SetBooleanPref(const char* pref_name, bool value) {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser || !browser->profile())
    return;

  PrefService* prefs = browser->profile()->GetPrefs();
  prefs->SetBoolean(pref_name, value);
}

void SetIntegerPref(const char* pref_name, int value) {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser || !browser->profile())
    return;

  PrefService* prefs = browser->profile()->GetPrefs();
  prefs->SetInteger(pref_name, value);
}

// Returns prefs::kShow3gPromoNotification or false
// if there's no active browser.
bool ShouldShow3gPromoNotification() {
  return GetBooleanPref(prefs::kShow3gPromoNotification);
}

void SetShow3gPromoNotification(bool value) {
  SetBooleanPref(prefs::kShow3gPromoNotification, value);
}
// Returns prefs::kCarrierDealPromoShown which is number of times
// carrier deal notification has been shown to user or -1
// if there's no active browser.
int GetCarrierDealPromoShown() {
  return GetIntegerPref(prefs::kCarrierDealPromoShown);
}

void SetCarrierDealPromoShown(int value) {
  SetIntegerPref(prefs::kCarrierDealPromoShown, value);
}

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton

// static
const int NetworkMenuButton::kThrobDuration = 1000;

NetworkMenuButton::NetworkMenuButton(StatusAreaHost* host)
    : StatusAreaButton(host, this),
      NetworkMenu(),
      icon_(NULL),
      right_badge_(NULL),
      left_badge_(NULL),
      mobile_data_bubble_(NULL),
      check_for_promo_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_connecting_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  animation_connecting_.SetThrobDuration(kThrobDuration);
  animation_connecting_.SetTweenType(ui::Tween::EASE_IN_OUT);
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  OnNetworkManagerChanged(network_library);
  network_library->AddNetworkManagerObserver(this);
  network_library->AddCellularDataPlanObserver(this);
  const NetworkDevice* cellular = network_library->FindCellularDevice();
  if (cellular) {
    cellular_device_path_ = cellular->device_path();
    network_library->AddNetworkDeviceObserver(cellular_device_path_, this);
  }
}

NetworkMenuButton::~NetworkMenuButton() {
  NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
  netlib->RemoveNetworkManagerObserver(this);
  netlib->RemoveObserverForAllNetworks(this);
  netlib->RemoveCellularDataPlanObserver(this);
  if (!cellular_device_path_.empty())
    netlib->RemoveNetworkDeviceObserver(cellular_device_path_, this);
  if (mobile_data_bubble_)
    mobile_data_bubble_->Close();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, ui::AnimationDelegate implementation:

void NetworkMenuButton::AnimationProgressed(const ui::Animation* animation) {
  if (animation == &animation_connecting_) {
    SetIconOnly(IconForNetworkConnecting(
        animation_connecting_.GetCurrentValue(), false));
    // No need to set the badge here, because it should already be set.
    SchedulePaint();
  } else {
    MenuButton::AnimationProgressed(animation);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary::NetworkDeviceObserver implementation:

void NetworkMenuButton::OnNetworkDeviceChanged(NetworkLibrary* cros,
                                               const NetworkDevice* device) {
  // Device status, such as SIMLock may have changed.
  OnNetworkChanged(cros, cros->active_network());
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::NetworkManagerObserver implementation:

void NetworkMenuButton::OnNetworkManagerChanged(NetworkLibrary* cros) {
  OnNetworkChanged(cros, cros->active_network());
  ShowOptionalMobileDataPromoNotification(cros);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::NetworkObserver implementation:
void NetworkMenuButton::OnNetworkChanged(NetworkLibrary* cros,
                                         const Network* network) {
  // This gets called on initialization, so any changes should be reflected
  // in CrosMock::SetNetworkLibraryStatusAreaExpectations().
  SetNetworkIcon(cros, network);
  RefreshNetworkObserver(cros);
  RefreshNetworkDeviceObserver(cros);
  SchedulePaint();
  UpdateMenu();
}

void NetworkMenuButton::OnCellularDataPlanChanged(NetworkLibrary* cros) {
  // Call OnNetworkManagerChanged which will update the icon.
  OnNetworkManagerChanged(cros);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkMenu implementation:

bool NetworkMenuButton::IsBrowserMode() const {
  return host_->GetScreenMode() == StatusAreaHost::kBrowserMode;
}

gfx::NativeWindow NetworkMenuButton::GetNativeWindow() const {
  return host_->GetNativeWindow();
}

void NetworkMenuButton::OpenButtonOptions() {
  host_->OpenButtonOptions(this);
}

bool NetworkMenuButton::ShouldOpenButtonOptions() const {
  return host_->ShouldOpenButtonOptions(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::View implementation:

void NetworkMenuButton::OnLocaleChanged() {
  NetworkLibrary* lib = CrosLibrary::Get()->GetNetworkLibrary();
  SetNetworkIcon(lib, lib->active_network());
}

////////////////////////////////////////////////////////////////////////////////
// MessageBubbleDelegate implementation:

void NetworkMenuButton::BubbleClosing(Bubble* bubble, bool closed_by_escape) {
  mobile_data_bubble_ = NULL;
}

bool NetworkMenuButton::CloseOnEscape() {
  return true;
}

bool NetworkMenuButton::FadeInOnShow() {
  return false;
}

void NetworkMenuButton::OnHelpLinkActivated() {
  // mobile_data_bubble_ will be set to NULL in callback.
  if (mobile_data_bubble_)
    mobile_data_bubble_->Close();
  if (!deal_url_.empty()) {
    Browser* browser = BrowserList::GetLastActive();
    if (!browser)
      return;
    browser->ShowSingletonTab(GURL(deal_url_));
    deal_url_.clear();
  } else {
    const Network* cellular =
        CrosLibrary::Get()->GetNetworkLibrary()->cellular_network();
    if (!cellular)
      return;
    ShowTabbedNetworkSettings(cellular);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, private methods

const ServicesCustomizationDocument::CarrierDeal*
NetworkMenuButton::GetCarrierDeal(
    NetworkLibrary* cros) {
  std::string carrier_id = cros->GetCellularHomeCarrierId();
  if (carrier_id.empty()) {
    LOG(ERROR) << "Empty carrier ID with a cellular connected.";
    return NULL;
  }

  ServicesCustomizationDocument* customization =
      ServicesCustomizationDocument::GetInstance();
  if (!customization->IsReady())
    return NULL;

  const ServicesCustomizationDocument::CarrierDeal* deal =
      customization->GetCarrierDeal(carrier_id, true);
  if (deal) {
    // Check deal for validity.
    int carrier_deal_promo_pref = GetCarrierDealPromoShown();
    if (carrier_deal_promo_pref >= deal->notification_count)
      return NULL;
    const std::string locale = g_browser_process->GetApplicationLocale();
    std::string deal_text = deal->GetLocalizedString(locale,
                                                     "notification_text");
    if (deal_text.empty())
      return NULL;
  }
  return deal;
}

void NetworkMenuButton::SetIconAndBadges(const SkBitmap* icon,
                                         const SkBitmap* right_badge,
                                         const SkBitmap* left_badge) {
  icon_ = icon;
  right_badge_ = right_badge;
  left_badge_ = left_badge;
  SetIcon(IconForDisplay(icon_, right_badge_, NULL /*no top_left_icon*/,
                         left_badge_));
}

void NetworkMenuButton::SetIconOnly(const SkBitmap* icon) {
  icon_ = icon;
  SetIcon(IconForDisplay(icon_, right_badge_, NULL /*no top_left_icon*/,
                         left_badge_));
}

void NetworkMenuButton::SetBadgesOnly(const SkBitmap* right_badge,
                                      const SkBitmap* left_badge) {
  right_badge_ = right_badge;
  left_badge_ = left_badge;
  SetIcon(IconForDisplay(icon_, right_badge_, NULL /*no top_left_icon*/,
                         left_badge_));
}

void NetworkMenuButton::SetNetworkIcon(NetworkLibrary* cros,
                                       const Network* network) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  if (!cros || !CrosLibrary::Get()->EnsureLoaded()) {
    SetIconAndBadges(rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0),
                     rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_WARNING),
                     NULL);
    SetTooltipText(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_STATUSBAR_NETWORK_NO_NETWORK_TOOLTIP)));
    return;
  }

  if (!cros->Connected() && !cros->Connecting()) {
    animation_connecting_.Stop();
    SetIconAndBadges(rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0),
                     rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED),
                     NULL);
    SetTooltipText(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_STATUSBAR_NETWORK_NO_NETWORK_TOOLTIP)));
    return;
  }

  if (cros->wifi_connecting() || cros->cellular_connecting()) {
    // Start the connecting animation if not running.
    if (!animation_connecting_.is_animating()) {
      animation_connecting_.Reset();
      animation_connecting_.StartThrobbing(-1);
      SetIconOnly(IconForNetworkConnecting(0, false));
    }
    const WirelessNetwork* wireless = NULL;
    if (cros->wifi_connecting()) {
      wireless = cros->wifi_network();
      SetBadgesOnly(NULL, NULL);
    } else {  // cellular_connecting
      wireless = cros->cellular_network();
      SetBadgesOnly(BadgeForNetworkTechnology(cros->cellular_network()), NULL);
    }
    SetTooltipText(UTF16ToWide(l10n_util::GetStringFUTF16(
        wireless->configuring() ? IDS_STATUSBAR_NETWORK_CONFIGURING_TOOLTIP
                                : IDS_STATUSBAR_NETWORK_CONNECTING_TOOLTIP,
        UTF8ToUTF16(wireless->name()))));
  } else {
    // Stop connecting animation since we are not connecting.
    animation_connecting_.Stop();
    // Only set the icon, if it is an active network that changed.
    if (network && network->is_active()) {
      const SkBitmap* right_badge(NULL);
      const SkBitmap* left_badge(NULL);
      if (cros->virtual_network())
        left_badge = rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE);
      if (network->type() == TYPE_ETHERNET) {
        SetIconAndBadges(rb.GetBitmapNamed(IDR_STATUSBAR_WIRED),
                         right_badge, left_badge);
        SetTooltipText(
            UTF16ToWide(l10n_util::GetStringFUTF16(
                IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET))));
      } else if (network->type() == TYPE_WIFI) {
        const WifiNetwork* wifi = static_cast<const WifiNetwork*>(network);
        SetIconAndBadges(IconForNetworkStrength(wifi, false),
                         right_badge, left_badge);
        SetTooltipText(UTF16ToWide(l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
            UTF8ToUTF16(wifi->name()))));
      } else if (network->type() == TYPE_CELLULAR) {
        const CellularNetwork* cellular =
            static_cast<const CellularNetwork*>(network);
        right_badge = BadgeForNetworkTechnology(cellular);
        SetIconAndBadges(IconForNetworkStrength(cellular, false),
                         right_badge, left_badge);
        SetTooltipText(UTF16ToWide(l10n_util::GetStringFUTF16(
            IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
            UTF8ToUTF16(cellular->name()))));
      }
    }
  }
}

void NetworkMenuButton::RefreshNetworkObserver(NetworkLibrary* cros) {
  const Network* network = cros->active_network();
  std::string new_network = network ? network->service_path() : std::string();
  if (active_network_ != new_network) {
    if (!active_network_.empty()) {
      cros->RemoveNetworkObserver(active_network_, this);
    }
    if (!new_network.empty()) {
      cros->AddNetworkObserver(new_network, this);
    }
    active_network_ = new_network;
  }
}

void NetworkMenuButton::RefreshNetworkDeviceObserver(NetworkLibrary* cros) {
  const NetworkDevice* cellular = cros->FindCellularDevice();
  std::string new_cellular_device_path = cellular ?
      cellular->device_path() : std::string();
  if (cellular_device_path_ != new_cellular_device_path) {
    if (!cellular_device_path_.empty()) {
      cros->RemoveNetworkDeviceObserver(cellular_device_path_, this);
    }
    if (!new_cellular_device_path.empty()) {
      cros->AddNetworkDeviceObserver(new_cellular_device_path, this);
    }
    cellular_device_path_ = new_cellular_device_path;
  }
}

void NetworkMenuButton::ShowOptionalMobileDataPromoNotification(
    NetworkLibrary* cros) {
  // Display one-time notification for non-Guest users on first use
  // of Mobile Data connection or if there's a carrier deal defined
  // show that even if user has already seen generic promo.
  if (IsBrowserMode() && !UserManager::Get()->IsLoggedInAsGuest() &&
      check_for_promo_ && BrowserList::GetLastActive() &&
      cros->cellular_connected() && !cros->ethernet_connected() &&
      !cros->wifi_connected()) {
    const ServicesCustomizationDocument::CarrierDeal* deal =
        GetCarrierDeal(cros);
    std::string deal_text;
    int carrier_deal_promo_pref = -1;
    if (deal) {
      carrier_deal_promo_pref = GetCarrierDealPromoShown();
      const std::string locale = g_browser_process->GetApplicationLocale();
      deal_text = deal->GetLocalizedString(locale, "notification_text");
      deal_url_ = deal->top_up_url;
    } else if (!ShouldShow3gPromoNotification()) {
      check_for_promo_ = false;
      return;
    }

    gfx::Rect button_bounds = GetScreenBounds();
    // StatusArea button Y position is usually -1, fix it so that
    // Contains() method for screen bounds works correctly.
    button_bounds.set_y(button_bounds.y() + 1);
    gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));

    // Chrome window is initialized in visible state off screen and then is
    // moved into visible screen area. Make sure that we're on screen
    // so that bubble is shown correctly.
    if (!screen_bounds.Contains(button_bounds)) {
      // If we're not on screen yet, delay notification display.
      // It may be shown earlier, on next NetworkLibrary callback processing.
      if (method_factory_.empty()) {
        MessageLoop::current()->PostDelayedTask(FROM_HERE,
            method_factory_.NewRunnableMethod(
                &NetworkMenuButton::ShowOptionalMobileDataPromoNotification,
                cros),
            kPromoShowDelayMs);
      }
      return;
    }

    // Add deal text if it's defined.
    std::wstring notification_text;
    std::wstring default_text =
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_3G_NOTIFICATION_MESSAGE));
    if (!deal_text.empty()) {
      notification_text = StringPrintf(L"%ls\n\n%ls",
                                       UTF8ToWide(deal_text).c_str(),
                                       default_text.c_str());
    } else {
      notification_text = default_text;
    }

    // Use deal URL if it's defined or general "Network Settings" URL.
    int link_message_id;
    if (deal_url_.empty())
      link_message_id = IDS_OFFLINE_NETWORK_SETTINGS;
    else
      link_message_id = IDS_STATUSBAR_NETWORK_VIEW_ACCOUNT;

    mobile_data_bubble_ = MessageBubble::Show(
        GetWidget(),
        button_bounds,
        BubbleBorder::TOP_RIGHT ,
        ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_NOTIFICATION_3G),
        notification_text,
        UTF16ToWide(l10n_util::GetStringUTF16(link_message_id)),
        this);

    check_for_promo_ = false;
    SetShow3gPromoNotification(false);
    if (carrier_deal_promo_pref != kNotificationCountPrefDefault)
      SetCarrierDealPromoShown(carrier_deal_promo_pref + 1);
  }
}

}  // namespace chromeos
