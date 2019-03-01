// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_icon.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/network_icon_image_source.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/tray/tray_constants.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/tether_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vector_icon_types.h"

using chromeos::NetworkState;
using chromeos::NetworkTypePattern;

namespace ash {
namespace network_icon {

namespace {

// class used for maintaining a map of network state and images.
class NetworkIconImpl {
 public:
  NetworkIconImpl(const std::string& path,
                  IconType icon_type,
                  const std::string& network_type);

  // Determines whether or not the associated network might be dirty and if so
  // updates and generates the icon. Does nothing if network no longer exists.
  void Update(const chromeos::NetworkState* network, bool show_vpn_badge);

  const gfx::ImageSkia& image() const { return image_; }

 private:
  // Updates |strength_index_| for wireless networks. Returns true if changed.
  bool UpdateWirelessStrengthIndex(const chromeos::NetworkState* network);

  // Updates the local state for cellular networks. Returns true if changed.
  bool UpdateCellularState(const chromeos::NetworkState* network);

  // Gets |badges| based on |network| and the current state.
  void GetBadges(const NetworkState* network, Badges* badges);

  // Gets the appropriate icon and badges and composites the image.
  void GenerateImage(const chromeos::NetworkState* network);

  // Network path, used for debugging.
  std::string network_path_;

  // Defines color theme and VPN badging
  const IconType icon_type_;

  // Cached state of the network when the icon was last generated.
  std::string connection_state_;
  int strength_index_ = -1;
  Badge technology_badge_ = {};
  bool show_vpn_badge_ = false;
  bool is_roaming_ = false;
  bool behind_captive_portal_ = false;

  // Generated icon image.
  gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIconImpl);
};

//------------------------------------------------------------------------------
// Maintain a static (global) icon map. Note: Icons are never destroyed;
// it is assumed that a finite and reasonable number of network icons will be
// created during a session.

typedef std::map<std::string, NetworkIconImpl*> NetworkIconMap;

NetworkIconMap* GetIconMapInstance(IconType icon_type, bool create) {
  typedef std::map<IconType, NetworkIconMap*> IconTypeMap;
  static IconTypeMap* s_icon_map = nullptr;
  if (s_icon_map == nullptr) {
    if (!create)
      return nullptr;
    s_icon_map = new IconTypeMap;
  }
  if (s_icon_map->count(icon_type) == 0) {
    if (!create)
      return nullptr;
    (*s_icon_map)[icon_type] = new NetworkIconMap;
  }
  return (*s_icon_map)[icon_type];
}

NetworkIconMap* GetIconMap(IconType icon_type) {
  return GetIconMapInstance(icon_type, true);
}

void PurgeIconMap(IconType icon_type,
                  const std::set<std::string>& network_paths) {
  NetworkIconMap* icon_map = GetIconMapInstance(icon_type, false);
  if (!icon_map)
    return;
  for (NetworkIconMap::iterator loop_iter = icon_map->begin();
       loop_iter != icon_map->end();) {
    NetworkIconMap::iterator cur_iter = loop_iter++;
    if (network_paths.count(cur_iter->first) == 0) {
      delete cur_iter->second;
      icon_map->erase(cur_iter);
    }
  }
}

//------------------------------------------------------------------------------
// Utilities for generating icon images.

// Amount to fade icons while connecting.
const double kConnectingImageAlpha = 0.5;

// Number of discrete images to use for alpha fade animation
const int kNumFadeImages = 10;

bool IsTrayIcon(IconType icon_type) {
  return icon_type == ICON_TYPE_TRAY_REGULAR ||
         icon_type == ICON_TYPE_TRAY_OOBE;
}

SkColor GetDefaultColorForIconType(IconType icon_type) {
  if (icon_type == ICON_TYPE_TRAY_REGULAR)
    return kTrayIconColor;
  if (icon_type == ICON_TYPE_TRAY_OOBE)
    return kOobeTrayIconColor;
  return kUnifiedMenuIconColor;
}

bool IconTypeIsDark(IconType icon_type) {
  // Dark icon is used for OOBE tray icon because the background is white.
  return icon_type == ICON_TYPE_TRAY_OOBE;
}

bool IconTypeHasVPNBadge(IconType icon_type) {
  return (icon_type != ICON_TYPE_LIST && icon_type != ICON_TYPE_MENU_LIST);
}

gfx::Size GetSizeForBaseIconSize(const gfx::Size& base_icon_size) {
  return base_icon_size;
}

gfx::ImageSkia CreateNetworkIconImage(const gfx::ImageSkia& icon,
                                      const Badges& badges) {
  return gfx::CanvasImageSource::MakeImageSkia<NetworkIconImageSource>(
      GetSizeForBaseIconSize(icon.size()), icon, badges);
}

//------------------------------------------------------------------------------
// Utilities for extracting icon images.

ImageType ImageTypeForNetworkType(const std::string& type) {
  if (NetworkTypePattern::WiFi().MatchesType(type))
    return ARCS;

  if (NetworkTypePattern::Mobile().MatchesType(type))
    return BARS;

  return NONE;
}

// Returns the network type, performing a check to see if Wi-Fi networks
// have an associated Tether network. Used to display the correct icon.
std::string GetEffectiveNetworkType(const NetworkState* network,
                                    IconType icon_type) {
  if (IsTrayIcon(icon_type) && network->type() == shill::kTypeWifi &&
      !network->tether_guid().empty()) {
    return chromeos::kTypeTether;
  }

  return network->type();
}

ImageType ImageTypeForNetwork(const NetworkState* network, IconType icon_type) {
  return ImageTypeForNetworkType(GetEffectiveNetworkType(network, icon_type));
}

gfx::Size GetSizeForIconType(IconType icon_type) {
  int size = kMenuIconSize;
  if (IsTrayIcon(icon_type)) {
    size = kUnifiedTrayIconSize;
  } else if (icon_type == ICON_TYPE_DEFAULT_VIEW) {
    size = kUnifiedFeaturePodVectorIconSize;
  }
  return gfx::Size(size, size);
}

int GetPaddingForIconType(IconType icon_type) {
  if (IsTrayIcon(icon_type))
    return kUnifiedTrayNetworkIconPadding;
  return kTrayNetworkIconPadding;
}

gfx::ImageSkia GetImageForIndex(ImageType image_type,
                                IconType icon_type,
                                int index) {
  return gfx::CanvasImageSource::MakeImageSkia<SignalStrengthImageSource>(
      image_type, GetDefaultColorForIconType(icon_type),
      GetSizeForIconType(icon_type), index, GetPaddingForIconType(icon_type));
}

gfx::ImageSkia* ConnectingWirelessImage(ImageType image_type,
                                        IconType icon_type,
                                        double animation) {
  static const int kImageCount = kNumNetworkImages - 1;
  static gfx::ImageSkia* s_bars_images_dark[kImageCount];
  static gfx::ImageSkia* s_bars_images_light[kImageCount];
  static gfx::ImageSkia* s_arcs_images_dark[kImageCount];
  static gfx::ImageSkia* s_arcs_images_light[kImageCount];
  int index = animation * nextafter(static_cast<float>(kImageCount), 0);
  index = std::max(std::min(index, kImageCount - 1), 0);
  gfx::ImageSkia** images;
  bool dark = IconTypeIsDark(icon_type);
  if (image_type == BARS)
    images = dark ? s_bars_images_dark : s_bars_images_light;
  else
    images = dark ? s_arcs_images_dark : s_arcs_images_light;
  if (!images[index]) {
    // Lazily cache images.
    // TODO(estade): should the alpha be applied in SignalStrengthImageSource?
    gfx::ImageSkia source = GetImageForIndex(image_type, icon_type, index + 1);
    images[index] =
        new gfx::ImageSkia(gfx::ImageSkiaOperations::CreateTransparentImage(
            source, kConnectingImageAlpha));
  }
  return images[index];
}

gfx::ImageSkia ConnectingVpnImage(double animation) {
  float floored_animation_value =
      std::floor(animation * kNumFadeImages) / kNumFadeImages;
  return gfx::CreateVectorIcon(
      kNetworkVpnIcon,
      gfx::Tween::ColorValueBetween(
          floored_animation_value,
          SkColorSetA(kMenuIconColor, kConnectingImageAlpha), kMenuIconColor));
}

int StrengthIndex(int strength) {
  if (strength <= 0)
    return 0;

  if (strength >= 100)
    return kNumNetworkImages - 1;

  // Return an index in the range [1, kNumNetworkImages - 1].
  // This logic is equivalent to cr_network_icon.js:strengthToIndex_().
  int zero_based_index = (strength - 1) * (kNumNetworkImages - 1) / 100;
  return zero_based_index + 1;
}

Badge BadgeForNetworkTechnology(const NetworkState* network,
                                IconType icon_type) {
  Badge badge = {nullptr, GetDefaultColorForIconType(icon_type)};
  const std::string& technology = network->network_technology();
  if (technology == shill::kNetworkTechnologyEvdo) {
    badge.icon = &kNetworkBadgeTechnologyEvdoIcon;
  } else if (technology == shill::kNetworkTechnology1Xrtt) {
    badge.icon = &kNetworkBadgeTechnology1xIcon;
  } else if (technology == shill::kNetworkTechnologyGprs ||
             technology == shill::kNetworkTechnologyGsm) {
    badge.icon = &kNetworkBadgeTechnologyGprsIcon;
  } else if (technology == shill::kNetworkTechnologyEdge) {
    badge.icon = &kNetworkBadgeTechnologyEdgeIcon;
  } else if (technology == shill::kNetworkTechnologyUmts) {
    badge.icon = &kNetworkBadgeTechnology3gIcon;
  } else if (technology == shill::kNetworkTechnologyHspa) {
    badge.icon = &kNetworkBadgeTechnologyHspaIcon;
  } else if (technology == shill::kNetworkTechnologyHspaPlus) {
    badge.icon = &kNetworkBadgeTechnologyHspaPlusIcon;
  } else if (technology == shill::kNetworkTechnologyLte) {
    badge.icon = &kNetworkBadgeTechnologyLteIcon;
  } else if (technology == shill::kNetworkTechnologyLteAdvanced) {
    badge.icon = &kNetworkBadgeTechnologyLteAdvancedIcon;
  } else {
    return {};
  }
  return badge;
}

gfx::ImageSkia GetIcon(const NetworkState* network,
                       IconType icon_type,
                       int strength_index) {
  if (network->Matches(NetworkTypePattern::Ethernet())) {
    return gfx::CreateVectorIcon(vector_icons::kEthernetIcon,
                                 GetDefaultColorForIconType(icon_type));
  }
  if (network->Matches(NetworkTypePattern::Wireless())) {
    return GetImageForIndex(ImageTypeForNetwork(network, icon_type), icon_type,
                            strength_index);
  }
  if (network->Matches(NetworkTypePattern::VPN())) {
    DCHECK(!IsTrayIcon(icon_type));
    return gfx::CreateVectorIcon(kNetworkVpnIcon,
                                 GetDefaultColorForIconType(ICON_TYPE_LIST));
  }

  NOTREACHED() << "Request for icon for unsupported type: " << network->type();
  return gfx::ImageSkia();
}

gfx::ImageSkia GetConnectingVpnImage(IconType icon_type) {
  double animation = NetworkIconAnimation::GetInstance()->GetAnimation();
  gfx::ImageSkia icon = ConnectingVpnImage(animation);
  return CreateNetworkIconImage(icon, Badges());
}

}  // namespace

//------------------------------------------------------------------------------
// NetworkIconImpl

NetworkIconImpl::NetworkIconImpl(const std::string& path,
                                 IconType icon_type,
                                 const std::string& network_type)
    : network_path_(path), icon_type_(icon_type) {
  // Default image is null.
}

void NetworkIconImpl::Update(const NetworkState* network, bool show_vpn_badge) {
  DCHECK(network);
  // Determine whether or not we need to update the icon.
  bool dirty = image_.isNull();

  std::string connection_state = network->connection_state();
  if (connection_state != connection_state_) {
    connection_state_ = connection_state;
    dirty = true;
  }

  bool behind_captive_portal = network->IsCaptivePortal();
  if (behind_captive_portal != behind_captive_portal_) {
    behind_captive_portal_ = behind_captive_portal;
    dirty = true;
  }

  if (network->Matches(NetworkTypePattern::Wireless()))
    dirty |= UpdateWirelessStrengthIndex(network);

  if (network->Matches(NetworkTypePattern::Cellular()))
    dirty |= UpdateCellularState(network);

  bool new_show_vpn_badge = show_vpn_badge && IconTypeHasVPNBadge(icon_type_);
  if (new_show_vpn_badge != show_vpn_badge_) {
    show_vpn_badge_ = new_show_vpn_badge;
    dirty = true;
  }

  if (dirty) {
    // Set the icon and badges based on the network and generate the image.
    GenerateImage(network);
  }
}

bool NetworkIconImpl::UpdateWirelessStrengthIndex(const NetworkState* network) {
  int index = StrengthIndex(network->signal_strength());
  if (index != strength_index_) {
    strength_index_ = index;
    return true;
  }
  return false;
}

bool NetworkIconImpl::UpdateCellularState(const NetworkState* network) {
  bool dirty = false;
  if (!features::IsSeparateNetworkIconsEnabled()) {
    const Badge technology_badge =
        BadgeForNetworkTechnology(network, icon_type_);
    if (technology_badge != technology_badge_) {
      technology_badge_ = technology_badge;
      dirty = true;
    }
  }
  bool is_roaming = network->IndicateRoaming();
  if (is_roaming != is_roaming_) {
    is_roaming_ = is_roaming;
    dirty = true;
  }
  return dirty;
}

void NetworkIconImpl::GetBadges(const NetworkState* network, Badges* badges) {
  DCHECK(network);

  const std::string& type = network->type();
  const SkColor icon_color = GetDefaultColorForIconType(icon_type_);
  if (type == shill::kTypeWifi) {
    if (network->security_class() != shill::kSecurityNone &&
        !IsTrayIcon(icon_type_)) {
      badges->bottom_right = {&kUnifiedNetworkBadgeSecureIcon, icon_color};
    }
  } else if (type == shill::kTypeWimax) {
    technology_badge_ = {&kNetworkBadgeTechnology4gIcon, icon_color};
  } else if (type == shill::kTypeCellular) {
    // technology_badge_ is set in UpdateCellularState.
    if (network->IsConnectedState() && network->IndicateRoaming())
      badges->bottom_right = {&kNetworkBadgeRoamingIcon, icon_color};
  }
  // Only show technology badge when connected.
  if (network->IsConnectedState() && !features::IsSeparateNetworkIconsEnabled())
    badges->top_left = technology_badge_;
  if (show_vpn_badge_)
    badges->bottom_left = {&kUnifiedNetworkBadgeVpnIcon, icon_color};
  if (behind_captive_portal_)
    badges->bottom_right = {&kUnifiedNetworkBadgeCaptivePortalIcon, icon_color};
}

void NetworkIconImpl::GenerateImage(const NetworkState* network) {
  DCHECK(network);
  gfx::ImageSkia icon = GetIcon(network, icon_type_, strength_index_);
  Badges badges;
  GetBadges(network, &badges);
  image_ = CreateNetworkIconImage(icon, badges);
}

namespace {

NetworkIconImpl* FindAndUpdateImageImpl(const NetworkState* network,
                                        IconType icon_type,
                                        bool show_vpn_badge) {
  // Find or add the icon.
  NetworkIconMap* icon_map = GetIconMap(icon_type);
  NetworkIconImpl* icon;
  NetworkIconMap::iterator iter = icon_map->find(network->path());
  if (iter == icon_map->end()) {
    icon = new NetworkIconImpl(network->path(), icon_type,
                               GetEffectiveNetworkType(network, icon_type));
    icon_map->insert(std::make_pair(network->path(), icon));
  } else {
    icon = iter->second;
  }

  // Update and return the icon's image.
  icon->Update(network, show_vpn_badge);
  return icon;
}

}  // namespace

//------------------------------------------------------------------------------
// Public interface

const gfx::ImageSkia GetBasicImage(IconType icon_type,
                                   const std::string& network_type,
                                   bool connected) {
  DCHECK_NE(shill::kTypeVPN, network_type);
  return GetImageForIndex(ImageTypeForNetworkType(network_type), icon_type,
                          connected ? kNumNetworkImages - 1 : 0);
}

gfx::ImageSkia GetImageForNonVirtualNetwork(const NetworkState* network,
                                            IconType icon_type,
                                            bool show_vpn_badge,
                                            bool* animating) {
  DCHECK(network);
  DCHECK(!network->Matches(NetworkTypePattern::VPN()));
  const std::string network_type = GetEffectiveNetworkType(network, icon_type);

  if (!network->visible()) {
    if (animating)
      *animating = false;
    return GetBasicImage(icon_type, network_type, false /* connected */);
  }

  if (network->IsConnectingState()) {
    if (animating)
      *animating = true;
    return GetConnectingImageForNetworkType(network_type, icon_type);
  }

  NetworkIconImpl* icon =
      FindAndUpdateImageImpl(network, icon_type, show_vpn_badge);
  if (animating)
    *animating = false;
  return icon->image();
}

gfx::ImageSkia GetImageForVPN(const NetworkState* vpn,
                              IconType icon_type,
                              bool* animating) {
  DCHECK(vpn);
  DCHECK(vpn->Matches(NetworkTypePattern::VPN()));
  if (vpn->IsConnectingState()) {
    if (animating)
      *animating = true;
    return GetConnectingVpnImage(icon_type);
  }

  NetworkIconImpl* icon =
      FindAndUpdateImageImpl(vpn, icon_type, false /* show_vpn_badge */);
  if (animating)
    *animating = false;
  return icon->image();
}

gfx::ImageSkia GetImageForWiFiEnabledState(bool enabled, IconType icon_type) {
  if (!enabled) {
    return gfx::CreateVectorIcon(kUnifiedMenuWifiOffIcon,
                                 GetSizeForIconType(icon_type).width(),
                                 GetDefaultColorForIconType(icon_type));
  }

  gfx::ImageSkia image =
      GetBasicImage(icon_type, shill::kTypeWifi, true /* connected */);
  Badges badges;
  if (!enabled) {
    badges.center = {&kNetworkBadgeOffIcon,
                     GetDefaultColorForIconType(icon_type)};
  }
  return CreateNetworkIconImage(image, badges);
}

gfx::ImageSkia GetConnectingImageForNetworkType(const std::string& network_type,
                                                IconType icon_type) {
  DCHECK(network_type != shill::kTypeVPN);
  ImageType image_type = ImageTypeForNetworkType(network_type);
  double animation = NetworkIconAnimation::GetInstance()->GetAnimation();

  return CreateNetworkIconImage(
      *ConnectingWirelessImage(image_type, icon_type, animation), Badges());
}

gfx::ImageSkia GetConnectedNetworkWithConnectingVpnImage(
    const NetworkState* connected_network,
    IconType icon_type) {
  DCHECK(connected_network);
  gfx::ImageSkia icon = GetImageForNonVirtualNetwork(
      connected_network, icon_type, false /* show_vpn_badge */);
  double animation = NetworkIconAnimation::GetInstance()->GetAnimation();
  Badges badges;
  badges.bottom_left = {
      &kUnifiedNetworkBadgeVpnIcon,
      SkColorSetA(GetDefaultColorForIconType(icon_type), 0xFF * animation)};
  return CreateNetworkIconImage(icon, badges);
}

gfx::ImageSkia GetDisconnectedImageForNetworkType(
    const std::string& network_type) {
  return GetBasicImage(ICON_TYPE_LIST, network_type, false /* connected */);
}

gfx::ImageSkia GetImageForNewWifiNetwork(SkColor icon_color,
                                         SkColor badge_color) {
  gfx::ImageSkia icon =
      gfx::CanvasImageSource::MakeImageSkia<SignalStrengthImageSource>(
          ImageTypeForNetworkType(shill::kTypeWifi), icon_color,
          GetSizeForIconType(ICON_TYPE_LIST), kNumNetworkImages - 1);
  Badges badges;
  badges.bottom_right = {&kNetworkBadgeAddOtherIcon, badge_color};
  return CreateNetworkIconImage(icon, badges);
}

base::string16 GetLabelForNetwork(const chromeos::NetworkState* network,
                                  IconType icon_type) {
  DCHECK(network);
  std::string activation_state = network->activation_state();
  if (icon_type == ICON_TYPE_LIST || icon_type == ICON_TYPE_MENU_LIST) {
    // Show "<network>: [Connecting|Activating]..."
    if (icon_type != ICON_TYPE_MENU_LIST && network->IsConnectingState()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_LIST_CONNECTING,
          base::UTF8ToUTF16(network->name()));
    }
    if (activation_state == shill::kActivationStateActivating) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_LIST_ACTIVATING,
          base::UTF8ToUTF16(network->name()));
    }
    // Show "Activate <network>" in list view only.
    if (activation_state == shill::kActivationStateNotActivated ||
        activation_state == shill::kActivationStatePartiallyActivated) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_LIST_ACTIVATE,
          base::UTF8ToUTF16(network->name()));
    }
  } else {
    // Show "[Connected to|Connecting to|Activating] <network>" (non-list view).
    if (network->IsConnectedState()) {
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED,
                                        base::UTF8ToUTF16(network->name()));
    }
    if (network->IsConnectingState()) {
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING,
                                        base::UTF8ToUTF16(network->name()));
    }
    if (activation_state == shill::kActivationStateActivating) {
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_ACTIVATING,
                                        base::UTF8ToUTF16(network->name()));
    }
  }

  // Otherwise just show the network name or 'Ethernet'.
  if (network->Matches(NetworkTypePattern::Ethernet())) {
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET);
  } else {
    return base::UTF8ToUTF16(network->name());
  }
}

void PurgeNetworkIconCache(const std::set<std::string>& network_paths) {
  PurgeIconMap(ICON_TYPE_TRAY_OOBE, network_paths);
  PurgeIconMap(ICON_TYPE_TRAY_REGULAR, network_paths);
  PurgeIconMap(ICON_TYPE_DEFAULT_VIEW, network_paths);
  PurgeIconMap(ICON_TYPE_LIST, network_paths);
  PurgeIconMap(ICON_TYPE_MENU_LIST, network_paths);
}

SignalStrength GetSignalStrengthForNetwork(
    const chromeos::NetworkState* network) {
  if (!network->Matches(NetworkTypePattern::Wireless()))
    return SignalStrength::NOT_WIRELESS;

  // Decide whether the signal is considered weak, medium or strong based on the
  // strength index. Each signal strength corresponds to a bucket which
  // attempted to be split evenly from |kNumNetworkImages| - 1. Remainders go
  // first to the lowest bucket and then the second lowest bucket.
  const int index = StrengthIndex(network->signal_strength());
  if (index == 0)
    return SignalStrength::NONE;
  const int seperations = kNumNetworkImages - 1;
  const int bucket_size = seperations / 3;

  const int weak_max = bucket_size + static_cast<int>(seperations % 3 != 0);
  const int medium_max =
      weak_max + bucket_size + static_cast<int>(seperations % 3 == 2);
  if (index <= weak_max)
    return SignalStrength::WEAK;
  else if (index <= medium_max)
    return SignalStrength::MEDIUM;

  return SignalStrength::STRONG;
}

}  // namespace network_icon
}  // namespace ash
