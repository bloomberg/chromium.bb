// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_icon.h"

#include "ash/shell.h"
#include "ash/system/chromeos/network/network_icon_animation.h"
#include "ash/system/chromeos/network/network_icon_animation_observer.h"
#include "base/utf_string_conversions.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size_conversions.h"

using chromeos::DeviceState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkState;

namespace ash {
namespace network_icon {

namespace {

//------------------------------------------------------------------------------
// Struct to pass icon badges to NetworkIconImageSource.
struct Badges {
  Badges()
      : top_left(NULL),
        top_right(NULL),
        bottom_left(NULL),
        bottom_right(NULL) {
  }
  const gfx::ImageSkia* top_left;
  const gfx::ImageSkia* top_right;
  const gfx::ImageSkia* bottom_left;
  const gfx::ImageSkia* bottom_right;
};

//------------------------------------------------------------------------------
// class used for maintaining a map of network state and images.
class NetworkIconImpl {
 public:
  explicit NetworkIconImpl(IconType icon_type);

  // Determines whether or not the associated network might be dirty and if so
  // updates and generates the icon. Does nothing if network no longer exists.
  void Update(const chromeos::NetworkState* network);

  const gfx::ImageSkia& image() const { return image_; }

 private:
  // Updates |strength_index_| for wireless networks. Returns true if changed.
  bool UpdateWirelessStrengthIndex(const chromeos::NetworkState* network);

  // Updates the local state for cellular networks. Returns true if changed.
  bool UpdateCellularState(const chromeos::NetworkState* network);

  // Updates the VPN badge. Returns true if changed.
  bool UpdateVPNBadge();

  // Gets |badges| based on |network| and the current state.
  void GetBadges(const NetworkState* network, Badges* badges);

  // Gets the appropriate icon and badges and composites the image.
  void GenerateImage(const chromeos::NetworkState* network);

  // Defines color theme and VPN badging
  const IconType icon_type_;

  // Cached state of the network when the icon was last generated.
  std::string state_;

  // Cached strength index of the network when the icon was last generated.
  int strength_index_;

  // Cached technology badge for the network when the icon was last generated.
  const gfx::ImageSkia* technology_badge_;

  // Cached vpn badge for the network when the icon was last generated.
  const gfx::ImageSkia* vpn_badge_;

  // Cached roaming state of the network when the icon was last generated.
  std::string roaming_state_;

  // Generated icon image.
  gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIconImpl);
};

//------------------------------------------------------------------------------
// Maintain a static (global) icon map. Note: Icons are never destroyed;
// it is assumed that a finite and reasonable number of network icons will be
// created during a session.

typedef std::map<std::string, NetworkIconImpl*> NetworkIconMap;

NetworkIconMap* GetIconMap(IconType icon_type) {
  typedef std::map<IconType, NetworkIconMap*> IconTypeMap;
  static IconTypeMap* s_icon_map = NULL;
  if (s_icon_map == NULL)
    s_icon_map = new IconTypeMap;
  if (s_icon_map->count(icon_type) == 0)
    (*s_icon_map)[icon_type] = new NetworkIconMap;
  return (*s_icon_map)[icon_type];
}

//------------------------------------------------------------------------------
// Utilities for generating icon images.

// 'NONE' will default to ARCS behavior where appropriate (e.g. no network or
// if a new type gets added).
enum ImageType {
  ARCS,
  BARS,
  NONE
};

// Amount to fade icons while connecting.
const double kConnectingImageAlpha = 0.5;

// Images for strength bars for wired networks.
const int kNumBarsImages = 5;
gfx::ImageSkia* kBarsImagesAnimatingDark[kNumBarsImages - 1];
gfx::ImageSkia* kBarsImagesAnimatingLight[kNumBarsImages - 1];

// Imagaes for strength arcs for wireless networks.
const int kNumArcsImages = 5;
gfx::ImageSkia* kArcsImagesAnimatingDark[kNumArcsImages - 1];
gfx::ImageSkia* kArcsImagesAnimatingLight[kNumArcsImages - 1];

//------------------------------------------------------------------------------
// Classes for generating scaled images.

const SkBitmap GetEmptyBitmap(const gfx::Size pixel_size) {
  typedef std::pair<int, int> SizeKey;
  typedef std::map<SizeKey, SkBitmap> SizeBitmapMap;
  static SizeBitmapMap* s_empty_bitmaps = new SizeBitmapMap;

  SizeKey key(pixel_size.width(), pixel_size.height());

  SizeBitmapMap::iterator iter = s_empty_bitmaps->find(key);
  if (iter != s_empty_bitmaps->end())
    return iter->second;

  SkBitmap empty;
  empty.setConfig(SkBitmap::kARGB_8888_Config, key.first, key.second);
  empty.allocPixels();
  empty.eraseARGB(0, 0, 0, 0);
  (*s_empty_bitmaps)[key] = empty;
  return empty;
}

class EmptyImageSource: public gfx::ImageSkiaSource {
 public:
  explicit EmptyImageSource(const gfx::Size& size)
      : size_(size) {
  }

  virtual gfx::ImageSkiaRep GetImageForScale(
      ui::ScaleFactor scale_factor) OVERRIDE {
    gfx::Size pixel_size = gfx::ToFlooredSize(
        gfx::ScaleSize(size_, ui::GetScaleFactorScale(scale_factor)));
    SkBitmap empty_bitmap = GetEmptyBitmap(pixel_size);
    return gfx::ImageSkiaRep(empty_bitmap, scale_factor);
  }
 private:
  const gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(EmptyImageSource);
};

// This defines how we assemble a network icon.
class NetworkIconImageSource : public gfx::ImageSkiaSource {
 public:
  NetworkIconImageSource(const gfx::ImageSkia& icon, const Badges& badges)
      : icon_(icon),
        badges_(badges) {
  }
  virtual ~NetworkIconImageSource() {}

  // TODO(pkotwicz): Figure out what to do when a new image resolution becomes
  // available.
  virtual gfx::ImageSkiaRep GetImageForScale(
      ui::ScaleFactor scale_factor) OVERRIDE {
    gfx::ImageSkiaRep icon_rep = icon_.GetRepresentation(scale_factor);
    if (icon_rep.is_null())
      return gfx::ImageSkiaRep();
    gfx::Canvas canvas(icon_rep, false);
    if (badges_.top_left)
      canvas.DrawImageInt(*badges_.top_left, 0, 0);
    if (badges_.top_right)
      canvas.DrawImageInt(*badges_.top_right,
                          icon_.width() - badges_.top_right->width(), 0);
    if (badges_.bottom_left) {
      canvas.DrawImageInt(*badges_.bottom_left,
                          0, icon_.height() - badges_.bottom_left->height());
    }
    if (badges_.bottom_right) {
      canvas.DrawImageInt(*badges_.bottom_right,
                          icon_.width() - badges_.bottom_right->width(),
                          icon_.height() - badges_.bottom_right->height());
    }
    return canvas.ExtractImageRep();
  }

 private:
  const gfx::ImageSkia icon_;
  const Badges badges_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIconImageSource);
};

//------------------------------------------------------------------------------
// Utilities for extracting icon images.

bool IconTypeIsDark(IconType icon_type) {
  return (icon_type != ICON_TYPE_TRAY);
}

bool IconTypeHasVPNBadge(IconType icon_type) {
  return (icon_type != ICON_TYPE_LIST);
}

int NumImagesForType(ImageType type) {
  return (type == BARS) ? kNumBarsImages : kNumArcsImages;
}

gfx::ImageSkia** ImageListForType(ImageType image_type, IconType icon_type) {
  gfx::ImageSkia** images;
  if (image_type == BARS) {
    images = IconTypeIsDark(icon_type) ?
        kBarsImagesAnimatingDark : kBarsImagesAnimatingLight;
  } else {
    images = IconTypeIsDark(icon_type) ?
        kArcsImagesAnimatingDark : kArcsImagesAnimatingLight;
  }
  return images;
}

gfx::ImageSkia* BaseImageForType(ImageType image_type, IconType icon_type) {
  gfx::ImageSkia* image;
  if (image_type == BARS) {
    image =  ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_BARS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_BARS_LIGHT);
  } else {
    image =  ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_ARCS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_ARCS_LIGHT);
  }
  return image;
}

ImageType ImageTypeForNetworkType(const std::string& type) {
  if (type == flimflam::kTypeWifi || type == flimflam::kTypeWimax)
    return ARCS;
  else if (type == flimflam::kTypeCellular)
    return BARS;
  return NONE;
}

gfx::ImageSkia GetImageForIndex(ImageType image_type,
                                IconType icon_type,
                                int index) {
  int num_images = NumImagesForType(image_type);
  if (index < 0 || index >= num_images)
    return gfx::ImageSkia();
  gfx::ImageSkia* images = BaseImageForType(image_type, icon_type);
  int width = images->width();
  int height = images->height() / num_images;
  return gfx::ImageSkiaOperations::ExtractSubset(*images,
      gfx::Rect(0, index * height, width, height));
}

const gfx::ImageSkia GetDisconnectedImage(const std::string& type,
                                          IconType icon_type) {
  ImageType image_type = ImageTypeForNetworkType(type);
  const int disconnected_index = 0;
  return GetImageForIndex(image_type, icon_type, disconnected_index);
}

int StrengthIndex(int strength, int count) {
  // Return an index in the range [1, count-1].
  const float findex = (static_cast<float>(strength) / 100.0f) *
      nextafter(static_cast<float>(count - 1), 0);
  int index = 1 + static_cast<int>(findex);
  index = std::max(std::min(index, count - 1), 1);
  return index;
}

int GetStrengthIndex(const NetworkState* network) {
  ImageType image_type = ImageTypeForNetworkType(network->type());
  if (image_type == ARCS)
    return StrengthIndex(network->signal_strength(), kNumArcsImages);
  else if (image_type == BARS)
    return StrengthIndex(network->signal_strength(), kNumBarsImages);
  return 0;
}

const gfx::ImageSkia* BadgeForNetworkTechnology(const NetworkState* network,
                                                IconType icon_type) {
  const int kUnknownBadgeType = -1;
  int id = kUnknownBadgeType;
  if (network->technology() == flimflam::kNetworkTechnologyEvdo) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_3G_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_3G_LIGHT;
  } else if (network->technology() == flimflam::kNetworkTechnology1Xrtt) {
    id = IDR_AURA_UBER_TRAY_NETWORK_1X;
  } else if (network->technology() == flimflam::kNetworkTechnologyGprs) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_GPRS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_GPRS_LIGHT;
  } else if (network->technology() == flimflam::kNetworkTechnologyEdge) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_EDGE_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_EDGE_LIGHT;
  } else if (network->technology() == flimflam::kNetworkTechnologyUmts) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_3G_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_3G_LIGHT;
  } else if (network->technology() == flimflam::kNetworkTechnologyHspa) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_HSPA_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_HSPA_LIGHT;
  } else if (network->technology() == flimflam::kNetworkTechnologyHspaPlus) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_HSPA_PLUS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_HSPA_PLUS_LIGHT;
  } else if (network->technology() == flimflam::kNetworkTechnologyLte) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_LTE_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_LTE_LIGHT;
  } else if (network->technology() == flimflam::kNetworkTechnologyLteAdvanced) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_LTE_ADVANCED_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_LTE_ADVANCED_LIGHT;
  } else if (network->technology() == flimflam::kNetworkTechnologyGsm) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_GPRS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_GPRS_LIGHT;
  }
  if (id == kUnknownBadgeType)
    return NULL;
  else
    return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

const gfx::ImageSkia* BadgeForVPN(IconType icon_type) {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_AURA_UBER_TRAY_NETWORK_VPN_BADGE);
}

gfx::ImageSkia GetIcon(const NetworkState* network,
                       IconType icon_type,
                       int strength_index) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const std::string& type = network->type();
  if (type == flimflam::kTypeEthernet) {
    return *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_WIRED);
  } else if (type == flimflam::kTypeWifi ||
             type == flimflam::kTypeWimax ||
             type == flimflam::kTypeCellular) {
    DCHECK(strength_index > 0);
    return GetImageForIndex(
        ImageTypeForNetworkType(type), icon_type, strength_index);
  } else if (type == flimflam::kTypeVPN) {
    return *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_VPN);
  } else {
    LOG(WARNING) << "Request for icon for unsupported type: " << type;
    return *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_WIRED);
  }
}

//------------------------------------------------------------------------------
// Get connecting images

gfx::ImageSkia GetConnectingImage(const std::string& type, IconType icon_type) {
  ImageType image_type = ImageTypeForNetworkType(type);
  int image_count = NumImagesForType(image_type) - 1;
  gfx::ImageSkia** images = ImageListForType(image_type, icon_type);
  double animation = NetworkIconAnimation::GetInstance()->GetAnimation();
  int index = animation * nextafter(static_cast<float>(image_count), 0);
  index = std::max(std::min(index, image_count - 1), 0);

  // Lazily cache images.
  if (!images[index]) {
    gfx::ImageSkia source = GetImageForIndex(image_type, icon_type, index + 1);
    images[index] = new gfx::ImageSkia(
        gfx::ImageSkiaOperations::CreateBlendedImage(
            gfx::ImageSkia(new EmptyImageSource(source.size()), source.size()),
            source,
            kConnectingImageAlpha));
  }
  gfx::ImageSkia& icon = *images[index];
  return gfx::ImageSkia(
      new NetworkIconImageSource(icon, Badges()), icon.size());
}

}  // namespace

//------------------------------------------------------------------------------
// NetworkIconImpl

NetworkIconImpl::NetworkIconImpl(IconType icon_type)
    : icon_type_(icon_type),
      strength_index_(-1),
      technology_badge_(NULL),
      vpn_badge_(NULL) {
  // Default image
  image_ = GetDisconnectedImage(flimflam::kTypeWifi, icon_type);
}

void NetworkIconImpl::Update(const NetworkState* network) {
  DCHECK(network);
  // Determine whether or not we need to update the icon.
  bool dirty = image_.isNull();

  // If the network state has changed, the icon needs updating.
  if (state_ != network->connection_state()) {
    state_ = network->connection_state();
    dirty = true;
  }

  const std::string& type = network->type();
  if (type != flimflam::kTypeEthernet)
    dirty |= UpdateWirelessStrengthIndex(network);

  if (type == flimflam::kTypeCellular)
    dirty |= UpdateCellularState(network);

  if (IconTypeHasVPNBadge(icon_type_) && type != flimflam::kTypeVPN)
    dirty |= UpdateVPNBadge();

  if (dirty) {
    // Set the icon and badges based on the network and generate the image.
    GenerateImage(network);
  }
}

bool NetworkIconImpl::UpdateWirelessStrengthIndex(const NetworkState* network) {
  int index = GetStrengthIndex(network);
  if (index != strength_index_) {
    strength_index_ = index;
    return true;
  }
  return false;
}

bool NetworkIconImpl::UpdateCellularState(const NetworkState* network) {
  bool dirty = false;
  const gfx::ImageSkia* technology_badge =
      BadgeForNetworkTechnology(network, icon_type_);
  if (technology_badge != technology_badge_) {
    technology_badge_ = technology_badge;
    dirty = true;
  }
  std::string roaming_state = network->roaming();
  if (roaming_state != roaming_state_) {
    roaming_state_ = roaming_state;
    dirty = true;
  }
  return dirty;
}

bool NetworkIconImpl::UpdateVPNBadge() {
  const NetworkState* vpn =
      chromeos::NetworkStateHandler::Get()->ConnectedNetworkByType(
          flimflam::kTypeVPN);
  if (vpn && vpn_badge_ == NULL) {
    vpn_badge_ = BadgeForVPN(icon_type_);
    return true;
  } else if (!vpn && vpn_badge_ != NULL) {
    vpn_badge_ = NULL;
    return true;
  }
  return false;
}

void NetworkIconImpl::GetBadges(const NetworkState* network, Badges* badges) {
  DCHECK(network);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  chromeos::NetworkStateHandler* handler = chromeos::NetworkStateHandler::Get();

  const std::string& type = network->type();
  if (type == flimflam::kTypeWifi) {
    if (network->security() != flimflam::kSecurityNone &&
        IconTypeIsDark(icon_type_)) {
      badges->bottom_right = rb.GetImageSkiaNamed(
          IDR_AURA_UBER_TRAY_NETWORK_SECURE_DARK);
    }
  } else if (type == flimflam::kTypeWimax) {
    badges->top_left = rb.GetImageSkiaNamed(
        IconTypeIsDark(icon_type_) ?
        IDR_AURA_UBER_TRAY_NETWORK_4G_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_4G_LIGHT);
  } else if (type == flimflam::kTypeCellular) {
    if (network->roaming() == flimflam::kRoamingStateRoaming) {
      // For networks that are always in roaming don't show roaming badge.
      const DeviceState* device =
          handler->GetDeviceState(network->device_path());
      DCHECK(device);
      if (!device->provider_requires_roaming()) {
        badges->bottom_right = rb.GetImageSkiaNamed(
            IconTypeIsDark(icon_type_) ?
            IDR_AURA_UBER_TRAY_NETWORK_ROAMING_DARK :
            IDR_AURA_UBER_TRAY_NETWORK_ROAMING_LIGHT);
      }
    }
  }
  if (!network->IsConnectingState()) {
    badges->top_left = technology_badge_;
    badges->bottom_left = vpn_badge_;
  }
}

void NetworkIconImpl::GenerateImage(const NetworkState* network) {
  DCHECK(network);
  gfx::ImageSkia icon = GetIcon(network, icon_type_, strength_index_);
  Badges badges;
  GetBadges(network, &badges);
  image_ = gfx::ImageSkia(
      new NetworkIconImageSource(icon, badges), icon.size());
}

//------------------------------------------------------------------------------
// Public interface

gfx::ImageSkia GetImageForNetwork(const NetworkState* network,
                                  IconType icon_type) {
  DCHECK(network);
  // Handle connecting icons.
  if (network->IsConnectingState()) {
    NetworkIconAnimation::GetInstance()->AddNetwork(network->path());
    return GetConnectingImage(network->type(), icon_type);
  }
  NetworkIconAnimation::GetInstance()->RemoveNetwork(network->path());

  // Find or add the icon.
  NetworkIconMap* icon_map = GetIconMap(icon_type);
  NetworkIconImpl* icon;
  NetworkIconMap::iterator iter = icon_map->find(network->path());
  if (iter == icon_map->end()) {
    icon = new NetworkIconImpl(icon_type);
    icon_map->insert(std::make_pair(network->path(), icon));
  } else {
    icon = iter->second;
  }

  // Update and return the icon's image.
  icon->Update(network);
  return icon->image();
}

gfx::ImageSkia GetImageForConnectingNetwork(IconType icon_type,
                                            const std::string& network_type) {
  return GetConnectingImage(network_type, icon_type);
}

gfx::ImageSkia GetImageForDisconnectedNetwork(IconType icon_type,
                                              const std::string& network_type) {
  return GetDisconnectedImage(network_type, icon_type);
}

string16 GetLabelForNetwork(const chromeos::NetworkState* network,
                            IconType icon_type) {
  DCHECK(network);
  if (icon_type == ICON_TYPE_LIST) {
    if (network->IsConnectingState()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_LIST_CONNECTING,
          UTF8ToUTF16(network->name()));
    }
  } else {
    if (network->IsConnectedState()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED, UTF8ToUTF16(network->name()));
    }
    if (network->IsConnectingState()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING, UTF8ToUTF16(network->name()));
    }
  }
  return UTF8ToUTF16(network->name());
}

}  // namespace network_icon
}  // namespace ash
