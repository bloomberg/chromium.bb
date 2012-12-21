// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_icon.h"

#include "ash/shell.h"
#include "ash/system/chromeos/network/network_icon_animation_observer.h"
#include "base/observer_list.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/throb_animation.h"
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
// NetworkIconImpl class used for maintaining a map of network state and images.

class NetworkIconImpl {
 public:
  NetworkIconImpl(const std::string& service_path, ResourceColorTheme color);

  // Determines whether or not the associated network might be dirty and if so
  // updates and generates the icon. Does nothing if network no longer exists.
  void Update(const chromeos::NetworkState* network);

  const gfx::ImageSkia& image() const { return image_; }

 private:
  // Updates |strength_index_| for wireless networks. Returns true if changed.
  bool UpdateWirelessStrengthIndex(const chromeos::NetworkState* network);

  // Updates the local state for cellular networks. Returns true if changed.
  bool UpdateCellularState(const chromeos::NetworkState* network);

  // Gets the appropriate icon and badges and composites the image.
  void GenerateImage(const chromeos::NetworkState* network);

  // Service path for the network this icon is associated with.
  std::string service_path_;

  // Color theme for the icon.
  ResourceColorTheme color_;

  // Cached state of the network when the icon was last generated.
  std::string state_;

  // Cached strength index of the network when the icon was last generated.
  int strength_index_;

  // Cached technology badge for the network when the icon was last generated.
  const gfx::ImageSkia* technology_badge_;

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

NetworkIconMap* GetIconMap(ResourceColorTheme color) {
  if (color == COLOR_DARK) {
    static NetworkIconMap* s_icon_map_dark = NULL;
    if (s_icon_map_dark == NULL)
      s_icon_map_dark = new NetworkIconMap;
    return s_icon_map_dark;
  } else {
    static NetworkIconMap* s_icon_map_light = NULL;
    if (s_icon_map_light == NULL)
      s_icon_map_light = new NetworkIconMap;
    return s_icon_map_light;
  }
}

//------------------------------------------------------------------------------
// Utilities for generating icon images.

enum ImageType {
  ARCS = 0,
  BARS
};

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

// Animation cycle length.
const int kThrobDurationMs = 750;

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

int NumImagesForType(ImageType type) {
  return (type == ARCS) ? kNumArcsImages : kNumBarsImages;
}

gfx::ImageSkia** ImageListForType(ImageType type,
                                  ResourceColorTheme color) {
  gfx::ImageSkia** images;
  if (type == ARCS) {
    images = (color == COLOR_DARK) ?
        kArcsImagesAnimatingDark : kArcsImagesAnimatingLight;
  } else {
    images = (color == COLOR_DARK) ?
        kBarsImagesAnimatingDark : kBarsImagesAnimatingLight;
  }
  return images;
}

gfx::ImageSkia* BaseImageForType(ImageType type,
                                 ResourceColorTheme color) {
  gfx::ImageSkia* image;
  if (type == ARCS) {
    image =  ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        color == COLOR_DARK ?
        IDR_AURA_UBER_TRAY_NETWORK_ARCS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_ARCS_LIGHT);
  } else {
    image =  ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        color == COLOR_DARK ?
        IDR_AURA_UBER_TRAY_NETWORK_BARS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_BARS_LIGHT);
  }
  return image;
}

gfx::ImageSkia GetImageForIndex(ImageType type,
                                ResourceColorTheme color,
                                int index) {
  int num_images = NumImagesForType(type);
  if (index < 0 || index >= num_images)
    return gfx::ImageSkia();
  gfx::ImageSkia* images = BaseImageForType(type, color);
  int width = images->width();
  int height = images->height() / num_images;
  return gfx::ImageSkiaOperations::ExtractSubset(*images,
      gfx::Rect(0, index * height, width, height));
}

const gfx::ImageSkia GetDisconnectedImage(ResourceColorTheme color) {
  return GetImageForIndex(ARCS, color, 0);
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
  if (network->type() == flimflam::kTypeWifi) {
    return StrengthIndex(network->signal_strength(), kNumArcsImages);
  } else if (network->type() == flimflam::kTypeWimax) {
    return StrengthIndex(network->signal_strength(), kNumBarsImages);
  } else if (network->type() == flimflam::kTypeCellular) {
    return StrengthIndex(network->signal_strength(), kNumBarsImages);
  }
  return 0;
}

const gfx::ImageSkia* BadgeForNetworkTechnology(const NetworkState* network,
                                                ResourceColorTheme color) {
  const int kUnknownBadgeType = -1;
  int id = kUnknownBadgeType;
  if (network->technology() == flimflam::kNetworkTechnologyEvdo) {
    id = (color == COLOR_DARK) ?
        IDR_AURA_UBER_TRAY_NETWORK_3G_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_3G_LIGHT;
  } else if (network->technology() == flimflam::kNetworkTechnology1Xrtt) {
    id = IDR_AURA_UBER_TRAY_NETWORK_1X;
  } else if (network->technology() == flimflam::kNetworkTechnologyGprs) {
    id = IDR_AURA_UBER_TRAY_NETWORK_GPRS;
  } else if (network->technology() == flimflam::kNetworkTechnologyEdge) {
    id = (color == COLOR_DARK) ?
        IDR_AURA_UBER_TRAY_NETWORK_EDGE_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_EDGE_LIGHT;
  } else if (network->technology() == flimflam::kNetworkTechnologyUmts) {
    id =  (color == COLOR_DARK) ?
        IDR_AURA_UBER_TRAY_NETWORK_3G_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_3G_LIGHT;
  } else if (network->technology() == flimflam::kNetworkTechnologyHspa) {
    id = IDR_AURA_UBER_TRAY_NETWORK_HSPA;
  } else if (network->technology() == flimflam::kNetworkTechnologyHspaPlus) {
    id = IDR_AURA_UBER_TRAY_NETWORK_HSPA_PLUS;
  } else if (network->technology() == flimflam::kNetworkTechnologyLte) {
    id = IDR_AURA_UBER_TRAY_NETWORK_LTE;
  } else if (network->technology() == flimflam::kNetworkTechnologyLteAdvanced) {
    id = IDR_AURA_UBER_TRAY_NETWORK_LTE_ADVANCED;
  } else if (network->technology() == flimflam::kNetworkTechnologyGsm) {
    id = IDR_AURA_UBER_TRAY_NETWORK_GPRS;
  }
  if (id == kUnknownBadgeType)
    return NULL;
  else
    return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

gfx::ImageSkia GetIcon(const NetworkState* network,
                       ResourceColorTheme color,
                       int strength_index) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const std::string& type = network->type();
  if (type == flimflam::kTypeEthernet) {
    return *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_WIRED);
  } else if (type == flimflam::kTypeWifi) {
    DCHECK(strength_index > 0);
    return GetImageForIndex(ARCS, color, strength_index);
  } else if (type == flimflam::kTypeWimax) {
    DCHECK(strength_index > 0);
    return GetImageForIndex(BARS, color, strength_index);
  } else if (type == flimflam::kTypeCellular) {
    DCHECK(strength_index > 0);
    return GetImageForIndex(BARS, color, strength_index);
  } else if (type == flimflam::kTypeVPN) {
    return *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_VPN);
  } else {
    LOG(WARNING) << "Request for icon for unsupported type: " << type;
    return *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_WIRED);
  }
}

void GetBadges(const NetworkState* network,
               ResourceColorTheme color,
               Badges* badges) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  chromeos::NetworkStateHandler* handler = chromeos::NetworkStateHandler::Get();

  bool use_dark_icons = color == COLOR_DARK;
  const std::string& type = network->type();
  if (type == flimflam::kTypeWifi) {
    if (network->security() != flimflam::kSecurityNone && use_dark_icons) {
      badges->bottom_right = rb.GetImageSkiaNamed(
          IDR_AURA_UBER_TRAY_NETWORK_SECURE_DARK);
    }
  } else if (type == flimflam::kTypeWimax) {
    badges->top_left = rb.GetImageSkiaNamed(
        use_dark_icons ?
        IDR_AURA_UBER_TRAY_NETWORK_4G_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_4G_LIGHT);
  } else if (type == flimflam::kTypeCellular) {
    if (network->roaming() == flimflam::kRoamingStateRoaming) {
      // For networks that are always in roaming don't show roaming badge.
      const DeviceState* device =
          handler->GetDeviceState(network->device_path());
      if (!device->provider_requires_roaming()) {
        badges->bottom_right = rb.GetImageSkiaNamed(
            use_dark_icons ?
            IDR_AURA_UBER_TRAY_NETWORK_ROAMING_DARK :
            IDR_AURA_UBER_TRAY_NETWORK_ROAMING_LIGHT);
      }
    }
    if (!network->IsConnectingState()) {
      badges->top_left = BadgeForNetworkTechnology(network, color);
    }
  }
}

//------------------------------------------------------------------------------
// Handle connecting images

class ConnectingAnimation : public ui::AnimationDelegate {
 public:
  ConnectingAnimation()
      : ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)) {
    // Set up the animation throbber.
    animation_.SetThrobDuration(kThrobDurationMs);
    animation_.SetTweenType(ui::Tween::LINEAR);
  }

  virtual ~ConnectingAnimation() {}

  // ui::AnimationDelegate implementation.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    if (animation == &animation_) {
      FOR_EACH_OBSERVER(AnimationObserver, observers_, NetworkIconChanged());
    }
  }

  double GetAnimation() {
    if (!animation_.is_animating()) {
      animation_.Reset();
      animation_.StartThrobbing(-1 /*throb indefinitely*/);
      return 0;
    }
    return animation_.GetCurrentValue();
  }

  void AddObserver(AnimationObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(AnimationObserver* observer) {
    observers_.RemoveObserver(observer);
    if (observers_.size() == 0)
      animation_.Stop();
  }

 private:
  ui::ThrobAnimation animation_;
  ObserverList<AnimationObserver> observers_;
};

ConnectingAnimation* GetConnectingAnimation() {
  static ConnectingAnimation* s_connecting_animation =
      new ConnectingAnimation();
  return s_connecting_animation;
}

gfx::ImageSkia GetConnectingImage(const std::string& type,
                                  ResourceColorTheme color) {
  ImageType image_type = (type == flimflam::kTypeWifi) ? ARCS : BARS;
  int image_count = NumImagesForType(image_type) - 1;
  gfx::ImageSkia** images = ImageListForType(image_type, color);
  double animation = GetConnectingAnimation()->GetAnimation();
  int index = animation * nextafter(static_cast<float>(image_count), 0);
  index = std::max(std::min(index, image_count - 1), 0);

  // Lazily cache images.
  if (!images[index]) {
    gfx::ImageSkia source = GetImageForIndex(image_type, color, index + 1);
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

NetworkIconImpl::NetworkIconImpl(const std::string& service_path,
                                 ResourceColorTheme color)
    : service_path_(service_path),
      color_(color),
      strength_index_(-1),
      technology_badge_(NULL) {
  // Default image
  image_ = GetDisconnectedImage(color);
}

void NetworkIconImpl::Update(const NetworkState* network) {
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
      BadgeForNetworkTechnology(network, color_);
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

void NetworkIconImpl::GenerateImage(const NetworkState* network) {
  gfx::ImageSkia icon = GetIcon(network, color_, strength_index_);
  Badges badges;
  GetBadges(network, color_, &badges);
  image_ = gfx::ImageSkia(
      new NetworkIconImageSource(icon, badges), icon.size());
}

//------------------------------------------------------------------------------
// Public interface

gfx::ImageSkia GetImageForNetwork(const NetworkState* network,
                                  ResourceColorTheme color,
                                  AnimationObserver* observer) {
  if (network->IsConnectingState()) {
    if (observer)
      GetConnectingAnimation()->AddObserver(observer);
    return GetConnectingImage(network->type(), color);
  }
  // Not connecting, remove observer.
  if (observer)
    GetConnectingAnimation()->RemoveObserver(observer);

  NetworkIconMap* icon_map = GetIconMap(color);

  // Find or add the icon.
  NetworkIconImpl* icon;
  NetworkIconMap::iterator iter = icon_map->find(network->path());
  if (iter == icon_map->end()) {
    icon = new NetworkIconImpl(network->path(), color);
    icon_map->insert(std::make_pair(network->path(), icon));
  } else {
    icon = iter->second;
  }

  // Update and return the icon's image.
  icon->Update(network);
  return icon->image();
}

}  // namespace network_icon
}  // namespace ash
