// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/network_icon.h"

#include "ash/common/system/chromeos/network/network_icon_animation.h"
#include "ash/common/system/chromeos/network/network_icon_animation_observer.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vector_icon_types.h"

using chromeos::DeviceState;
using chromeos::NetworkConnectionHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkPortalDetector;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace network_icon {

namespace {

// Constants for offseting the badge displayed on top of the signal strength
// icon. The badge will extend outside of the base icon bounds by these amounts.
// All values are in dp.

// The badge offsets are different depending on whether the icon is in the tray
// or menu.
const int kTrayIconBadgeOffset = 3;
const int kMenuIconBadgeOffset = 2;

//------------------------------------------------------------------------------
// Struct to pass icon badges to NetworkIconImageSource.
struct Badges {
  gfx::ImageSkia top_left;
  gfx::ImageSkia top_right;
  gfx::ImageSkia bottom_left;
  gfx::ImageSkia bottom_right;
};

//------------------------------------------------------------------------------
// class used for maintaining a map of network state and images.
class NetworkIconImpl {
 public:
  NetworkIconImpl(const std::string& path, IconType icon_type);

  // Determines whether or not the associated network might be dirty and if so
  // updates and generates the icon. Does nothing if network no longer exists.
  void Update(const chromeos::NetworkState* network);

  const gfx::ImageSkia& image() const { return image_; }

 private:
  // Updates |strength_index_| for wireless networks. Returns true if changed.
  bool UpdateWirelessStrengthIndex(const chromeos::NetworkState* network);

  // Updates the local state for cellular networks. Returns true if changed.
  bool UpdateCellularState(const chromeos::NetworkState* network);

  // Updates the portal state for wireless networks. Returns true if changed.
  bool UpdatePortalState(const chromeos::NetworkState* network);

  // Updates the VPN badge. Returns true if changed.
  bool UpdateVPNBadge();

  // Gets |badges| based on |network| and the current state.
  void GetBadges(const NetworkState* network, Badges* badges);

  // Gets the appropriate icon and badges and composites the image.
  void GenerateImage(const chromeos::NetworkState* network);

  // Network path, used for debugging.
  std::string network_path_;

  // Defines color theme and VPN badging
  const IconType icon_type_;

  // Cached state of the network when the icon was last generated.
  std::string state_;

  // Cached strength index of the network when the icon was last generated.
  int strength_index_;

  // Cached technology badge for the network when the icon was last generated.
  gfx::ImageSkia technology_badge_;

  // Cached vpn badge for the network when the icon was last generated.
  gfx::ImageSkia vpn_badge_;

  // Cached roaming state of the network when the icon was last generated.
  std::string roaming_state_;

  // Cached portal state of the network when the icon was last generated.
  bool behind_captive_portal_;

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

// 'NONE' will default to ARCS behavior where appropriate (e.g. no network or
// if a new type gets added).
enum ImageType { ARCS, BARS, NONE };

// Amount to fade icons while connecting.
const double kConnectingImageAlpha = 0.5;

// Images for strength arcs for wireless networks or strength bars for cellular
// networks.
const int kNumNetworkImages = 5;

// Number of discrete images to use for alpha fade animation
const int kNumFadeImages = 10;

SkColor GetDefaultColorForIconType(IconType icon_type) {
  return icon_type == ICON_TYPE_TRAY ? kTrayIconColor : kMenuIconColor;
}

bool IconTypeIsDark(IconType icon_type) {
  return (icon_type != ICON_TYPE_TRAY);
}

bool IconTypeHasVPNBadge(IconType icon_type) {
  return (icon_type != ICON_TYPE_LIST && icon_type != ICON_TYPE_MENU_LIST);
}

// This defines how we assemble a network icon.
class NetworkIconImageSource : public gfx::CanvasImageSource {
 public:
  static gfx::ImageSkia CreateImage(const gfx::ImageSkia& icon,
                                    const Badges& badges) {
    auto* source = new NetworkIconImageSource(icon, badges);
    return gfx::ImageSkia(source, source->size());
  }

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    const int width = size().width();
    const int height = size().height();

    // The base icon is centered in both dimensions.
    const int icon_y = (height - icon_.height()) / 2;
    canvas->DrawImageInt(icon_, (width - icon_.width()) / 2, icon_y);

    // The badges are flush against the edges of the canvas, except at the top,
    // where the badge is only 1dp higher than the base image.
    const int top_badge_y = icon_y - 1;
    if (!badges_.top_left.isNull())
      canvas->DrawImageInt(badges_.top_left, 0, top_badge_y);
    if (!badges_.top_right.isNull()) {
      canvas->DrawImageInt(badges_.top_right, width - badges_.top_right.width(),
                           top_badge_y);
    }
    if (!badges_.bottom_left.isNull()) {
      canvas->DrawImageInt(badges_.bottom_left, 0,
                           height - badges_.bottom_left.height());
    }
    if (!badges_.bottom_right.isNull()) {
      canvas->DrawImageInt(badges_.bottom_right,
                           width - badges_.bottom_right.width(),
                           height - badges_.bottom_right.height());
    }
  }

  bool HasRepresentationAtAllScales() const override { return true; }

 private:
  NetworkIconImageSource(const gfx::ImageSkia& icon, const Badges& badges)
      : CanvasImageSource(GetSizeForBaseIconSize(icon.size()), false),
        icon_(icon),
        badges_(badges) {}
  ~NetworkIconImageSource() override {}

  static gfx::Size GetSizeForBaseIconSize(const gfx::Size& base_icon_size) {
    gfx::Size size = base_icon_size;
    const int badge_offset = base_icon_size.width() == kTrayIconSize
                                 ? kTrayIconBadgeOffset
                                 : kMenuIconBadgeOffset;
    size.Enlarge(badge_offset * 2, badge_offset * 2);
    return size;
  }

  const gfx::ImageSkia icon_;
  const Badges badges_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIconImageSource);
};

// Depicts a given signal strength using arcs (e.g. for WiFi connections) or
// bars (e.g. for cell connections).
class SignalStrengthImageSource : public gfx::CanvasImageSource {
 public:
  SignalStrengthImageSource(ImageType image_type,
                            IconType icon_type,
                            int signal_strength)
      : CanvasImageSource(GetSizeForIconType(icon_type), false),
        image_type_(image_type),
        icon_type_(icon_type),
        color_(GetDefaultColorForIconType(icon_type_)),
        signal_strength_(signal_strength) {
    if (image_type_ == NONE)
      image_type_ = ARCS;

    DCHECK_GE(signal_strength, 0);
    DCHECK_LT(signal_strength, kNumNetworkImages);
  }
  ~SignalStrengthImageSource() override {}

  void set_color(SkColor color) { color_ = color; }

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    if (image_type_ == ARCS)
      DrawArcs(canvas);
    else
      DrawBars(canvas);
  }

  bool HasRepresentationAtAllScales() const override { return true; }

 private:
  static gfx::Size GetSizeForIconType(IconType icon_type) {
    int side = icon_type == ICON_TYPE_TRAY ? kTrayIconSize : kMenuIconSize;
    return gfx::Size(side, side);
  }

  void DrawArcs(gfx::Canvas* canvas) {
    gfx::RectF oval_bounds((gfx::Rect(size())));
    oval_bounds.Inset(gfx::Insets(kIconInset));
    // Double the width and height. The new midpoint should be the former
    // bottom center.
    oval_bounds.Inset(-oval_bounds.width() / 2, 0, -oval_bounds.width() / 2,
                      -oval_bounds.height());

    const SkScalar kAngleAboveHorizontal = 51.f;
    const SkScalar kStartAngle = 180.f + kAngleAboveHorizontal;
    const SkScalar kSweepAngle = 180.f - 2 * kAngleAboveHorizontal;

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    // Background. Skip drawing for full signal.
    if (signal_strength_ != kNumNetworkImages - 1) {
      flags.setColor(SkColorSetA(color_, kBgAlpha));
      canvas->sk_canvas()->drawArc(gfx::RectFToSkRect(oval_bounds), kStartAngle,
                                   kSweepAngle, true, flags);
    }
    // Foreground (signal strength).
    if (signal_strength_ != 0) {
      flags.setColor(color_);
      // Percent of the height of the background wedge that we draw the
      // foreground wedge, indexed by signal strength.
      static const float kWedgeHeightPercentages[] = {0.f, 0.375f, 0.5833f,
                                                      0.75f, 1.f};
      const float wedge_percent = kWedgeHeightPercentages[signal_strength_];
      oval_bounds.Inset(
          gfx::InsetsF((oval_bounds.height() / 2) * (1.f - wedge_percent)));
      canvas->sk_canvas()->drawArc(gfx::RectFToSkRect(oval_bounds), kStartAngle,
                                   kSweepAngle, true, flags);
    }
  }

  void DrawBars(gfx::Canvas* canvas) {
    // Undo the canvas's device scaling and round values to the nearest whole
    // number so we can draw on exact pixel boundaries.
    const float dsf = canvas->UndoDeviceScaleFactor();
    auto scale = [dsf](SkScalar dimension) {
      return std::round(dimension * dsf);
    };

    // Length of short side of an isosceles right triangle, in dip.
    const SkScalar kFullTriangleSide =
        SkIntToScalar(size().width()) - kIconInset * 2;

    auto make_triangle = [scale, kFullTriangleSide](SkScalar side) {
      SkPath triangle;
      triangle.moveTo(scale(kIconInset), scale(kIconInset + kFullTriangleSide));
      triangle.rLineTo(scale(side), 0);
      triangle.rLineTo(0, -scale(side));
      triangle.close();
      return triangle;
    };

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    // Background. Skip drawing for full signal.
    if (signal_strength_ != kNumNetworkImages - 1) {
      flags.setColor(SkColorSetA(color_, kBgAlpha));
      canvas->DrawPath(make_triangle(kFullTriangleSide), flags);
    }
    // Foreground (signal strength).
    if (signal_strength_ != 0) {
      flags.setColor(color_);
      // As a percentage of the bg triangle, the length of one of the short
      // sides of the fg triangle, indexed by signal strength.
      static const float kTriangleSidePercents[] = {0.f, 0.5f, 0.625f, 0.75f,
                                                    1.f};
      canvas->DrawPath(make_triangle(kTriangleSidePercents[signal_strength_] *
                                     kFullTriangleSide),
                       flags);
    }
  }

  ImageType image_type_;
  IconType icon_type_;
  SkColor color_;

  // On a scale of 0 to kNum{Arcs,Bars}Images - 1, how connected we are.
  int signal_strength_;

  // Padding between outside of icon and edge of the canvas, in dp. This value
  // stays the same regardless of the canvas size (which depends on
  // |icon_type_|).
  static constexpr int kIconInset = 2;

  // TODO(estade): share this alpha with other things in ash (battery, etc.).
  // See crbug.com/623987 and crbug.com/632827
  static constexpr int kBgAlpha = 0x4D;

  DISALLOW_COPY_AND_ASSIGN(SignalStrengthImageSource);
};

//------------------------------------------------------------------------------
// Utilities for extracting icon images.

ImageType ImageTypeForNetworkType(const std::string& type) {
  if (type == shill::kTypeWifi)
    return ARCS;
  else if (type == shill::kTypeCellular || type == shill::kTypeWimax)
    return BARS;
  return NONE;
}

gfx::ImageSkia GetImageForIndex(ImageType image_type,
                                IconType icon_type,
                                int index) {
  gfx::CanvasImageSource* source =
      new SignalStrengthImageSource(image_type, icon_type, index);
  return gfx::ImageSkia(source, source->size());
}

const gfx::ImageSkia GetDisconnectedImage(IconType icon_type,
                                          const std::string& network_type) {
  DCHECK_NE(shill::kTypeVPN, network_type);
  ImageType image_type = ImageTypeForNetworkType(network_type);
  const int disconnected_index = 0;
  return GetImageForIndex(image_type, icon_type, disconnected_index);
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
  int index = animation * nextafter(static_cast<float>(kNumFadeImages), 0);
  static gfx::ImageSkia* s_vpn_images[kNumFadeImages];
  if (!s_vpn_images[index]) {
    // Lazily cache images.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    // TODO(estade): update this icon to MD. See crbug.com/690176
    gfx::ImageSkia* icon = rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_VPN);
    s_vpn_images[index] = new gfx::ImageSkia(
        gfx::ImageSkiaOperations::CreateTransparentImage(*icon, animation));
  }
  return *s_vpn_images[index];
}

gfx::ImageSkia ConnectingVpnBadge(double animation, IconType icon_type) {
  int index = animation * nextafter(static_cast<float>(kNumFadeImages), 0);
  static gfx::ImageSkia* s_vpn_badges[kNumFadeImages];
  if (!s_vpn_badges[index]) {
    // Lazily cache images.
    gfx::ImageSkia badge = gfx::CreateVectorIcon(
        kNetworkBadgeVpnIcon, GetDefaultColorForIconType(icon_type));
    s_vpn_badges[index] = new gfx::ImageSkia(
        gfx::ImageSkiaOperations::CreateTransparentImage(badge, animation));
  }
  return *s_vpn_badges[index];
}

int StrengthIndex(int strength) {
  // Return an index in the range [1, kNumNetworkImages - 1].
  const float findex = (static_cast<float>(strength) / 100.0f) *
                       nextafter(static_cast<float>(kNumNetworkImages - 1), 0);
  int index = 1 + static_cast<int>(findex);
  index = std::max(std::min(index, kNumNetworkImages - 1), 1);
  return index;
}

gfx::ImageSkia BadgeForNetworkTechnology(const NetworkState* network,
                                         IconType icon_type) {
  const std::string& technology = network->network_technology();
  const gfx::VectorIcon* icon = &gfx::kNoneIcon;
  if (technology == shill::kNetworkTechnologyEvdo) {
    icon = &kNetworkBadgeTechnologyEvdoIcon;
  } else if (technology == shill::kNetworkTechnology1Xrtt) {
    icon = &kNetworkBadgeTechnology1xIcon;
  } else if (technology == shill::kNetworkTechnologyGprs ||
             technology == shill::kNetworkTechnologyGsm) {
    icon = &kNetworkBadgeTechnologyGprsIcon;
  } else if (technology == shill::kNetworkTechnologyEdge) {
    icon = &kNetworkBadgeTechnologyEdgeIcon;
  } else if (technology == shill::kNetworkTechnologyUmts) {
    icon = &kNetworkBadgeTechnology3gIcon;
  } else if (technology == shill::kNetworkTechnologyHspa) {
    icon = &kNetworkBadgeTechnologyHspaIcon;
  } else if (technology == shill::kNetworkTechnologyHspaPlus) {
    icon = &kNetworkBadgeTechnologyHspaPlusIcon;
  } else if (technology == shill::kNetworkTechnologyLte) {
    icon = &kNetworkBadgeTechnologyLteIcon;
  } else if (technology == shill::kNetworkTechnologyLteAdvanced) {
    icon = &kNetworkBadgeTechnologyLteAdvancedIcon;
  } else {
    return gfx::ImageSkia();
  }
  return gfx::CreateVectorIcon(*icon, GetDefaultColorForIconType(icon_type));
}

gfx::ImageSkia GetIcon(const NetworkState* network,
                       IconType icon_type,
                       int strength_index) {
  if (network->Matches(NetworkTypePattern::Ethernet())) {
    DCHECK_NE(ICON_TYPE_TRAY, icon_type);
    return gfx::CreateVectorIcon(kNetworkEthernetIcon,
                                 GetDefaultColorForIconType(ICON_TYPE_LIST));
  } else if (network->Matches(NetworkTypePattern::Wireless())) {
    DCHECK(strength_index > 0);
    return GetImageForIndex(ImageTypeForNetworkType(network->type()), icon_type,
                            strength_index);
  } else if (network->Matches(NetworkTypePattern::VPN())) {
    DCHECK_NE(ICON_TYPE_TRAY, icon_type);
    return gfx::CreateVectorIcon(kNetworkVpnIcon,
                                 GetDefaultColorForIconType(ICON_TYPE_LIST));
  }

  NOTREACHED() << "Request for icon for unsupported type: " << network->type();
  return gfx::ImageSkia();
}

//------------------------------------------------------------------------------
// Get connecting images

gfx::ImageSkia GetConnectingVpnImage(IconType icon_type) {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  const NetworkState* connected_network = nullptr;
  if (icon_type == ICON_TYPE_TRAY) {
    connected_network =
        handler->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());
  }
  double animation = NetworkIconAnimation::GetInstance()->GetAnimation();

  gfx::ImageSkia icon;
  Badges badges;
  if (connected_network) {
    icon = GetImageForNetwork(connected_network, icon_type);
    badges.bottom_left = ConnectingVpnBadge(animation, icon_type);
  } else {
    icon = ConnectingVpnImage(animation);
  }
  return NetworkIconImageSource::CreateImage(icon, badges);
}

gfx::ImageSkia GetConnectingImage(IconType icon_type,
                                  const std::string& network_type) {
  if (network_type == shill::kTypeVPN)
    return GetConnectingVpnImage(icon_type);

  ImageType image_type = ImageTypeForNetworkType(network_type);
  double animation = NetworkIconAnimation::GetInstance()->GetAnimation();

  return NetworkIconImageSource::CreateImage(
      *ConnectingWirelessImage(image_type, icon_type, animation), Badges());
}

}  // namespace

//------------------------------------------------------------------------------
// NetworkIconImpl

NetworkIconImpl::NetworkIconImpl(const std::string& path, IconType icon_type)
    : network_path_(path),
      icon_type_(icon_type),
      strength_index_(-1),
      behind_captive_portal_(false) {
  // Default image
  image_ = GetDisconnectedImage(icon_type, shill::kTypeWifi);
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

  dirty |= UpdatePortalState(network);

  if (network->Matches(NetworkTypePattern::Wireless())) {
    dirty |= UpdateWirelessStrengthIndex(network);
  }

  if (network->Matches(NetworkTypePattern::Cellular()))
    dirty |= UpdateCellularState(network);

  if (IconTypeHasVPNBadge(icon_type_) &&
      network->Matches(NetworkTypePattern::NonVirtual())) {
    dirty |= UpdateVPNBadge();
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
  const gfx::ImageSkia technology_badge =
      BadgeForNetworkTechnology(network, icon_type_);
  if (!technology_badge.BackedBySameObjectAs(technology_badge_)) {
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

bool NetworkIconImpl::UpdatePortalState(const NetworkState* network) {
  bool behind_captive_portal = false;
  if (network && chromeos::network_portal_detector::IsInitialized()) {
    NetworkPortalDetector::CaptivePortalState state =
        chromeos::network_portal_detector::GetInstance()->GetCaptivePortalState(
            network->guid());
    behind_captive_portal =
        state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  }

  if (behind_captive_portal == behind_captive_portal_)
    return false;
  behind_captive_portal_ = behind_captive_portal;
  return true;
}

bool NetworkIconImpl::UpdateVPNBadge() {
  const NetworkState* vpn =
      NetworkHandler::Get()->network_state_handler()->ConnectedNetworkByType(
          NetworkTypePattern::VPN());
  if (vpn && vpn_badge_.isNull()) {
    vpn_badge_ = gfx::CreateVectorIcon(kNetworkBadgeVpnIcon,
                                       GetDefaultColorForIconType(icon_type_));
    return true;
  }
  if (!vpn && !vpn_badge_.isNull()) {
    vpn_badge_ = gfx::ImageSkia();
    return true;
  }
  return false;
}

void NetworkIconImpl::GetBadges(const NetworkState* network, Badges* badges) {
  DCHECK(network);

  const std::string& type = network->type();
  const SkColor icon_color = GetDefaultColorForIconType(icon_type_);
  if (type == shill::kTypeWifi) {
    if (network->security_class() != shill::kSecurityNone &&
        IconTypeIsDark(icon_type_)) {
      badges->bottom_right =
          gfx::CreateVectorIcon(kNetworkBadgeSecureIcon, icon_color);
    }
  } else if (type == shill::kTypeWimax) {
    technology_badge_ =
        gfx::CreateVectorIcon(kNetworkBadgeTechnology4gIcon, icon_color);
  } else if (type == shill::kTypeCellular) {
    if (network->roaming() == shill::kRoamingStateRoaming) {
      // For networks that are always in roaming don't show roaming badge.
      const DeviceState* device =
          NetworkHandler::Get()->network_state_handler()->GetDeviceState(
              network->device_path());
      LOG_IF(WARNING, !device) << "Could not find device state for "
                               << network->device_path();
      if (!device || !device->provider_requires_roaming()) {
        badges->bottom_right =
            gfx::CreateVectorIcon(kNetworkBadgeRoamingIcon, icon_color);
      }
    }
  }
  if (!network->IsConnectingState()) {
    badges->top_left = technology_badge_;
    badges->bottom_left = vpn_badge_;
  }

  if (behind_captive_portal_) {
    badges->bottom_right =
        gfx::CreateVectorIcon(kNetworkBadgeCaptivePortalIcon, icon_color);
  }
}

void NetworkIconImpl::GenerateImage(const NetworkState* network) {
  DCHECK(network);
  gfx::ImageSkia icon = GetIcon(network, icon_type_, strength_index_);
  Badges badges;
  GetBadges(network, &badges);
  image_ = NetworkIconImageSource::CreateImage(icon, badges);
}

namespace {

NetworkIconImpl* FindAndUpdateImageImpl(const NetworkState* network,
                                        IconType icon_type) {
  // Find or add the icon.
  NetworkIconMap* icon_map = GetIconMap(icon_type);
  NetworkIconImpl* icon;
  NetworkIconMap::iterator iter = icon_map->find(network->path());
  if (iter == icon_map->end()) {
    icon = new NetworkIconImpl(network->path(), icon_type);
    icon_map->insert(std::make_pair(network->path(), icon));
  } else {
    icon = iter->second;
  }

  // Update and return the icon's image.
  icon->Update(network);
  return icon;
}

}  // namespace

//------------------------------------------------------------------------------
// Public interface

gfx::ImageSkia GetImageForNetwork(const NetworkState* network,
                                  IconType icon_type) {
  DCHECK(network);
  if (!network->visible())
    return GetDisconnectedImage(icon_type, network->type());

  if (network->IsConnectingState())
    return GetConnectingImage(icon_type, network->type());

  NetworkIconImpl* icon = FindAndUpdateImageImpl(network, icon_type);
  return icon->image();
}

gfx::ImageSkia GetImageForConnectedMobileNetwork() {
  ImageType image_type = ImageTypeForNetworkType(shill::kTypeWifi);
  const IconType icon_type = ICON_TYPE_LIST;
  const int connected_index = kNumNetworkImages - 1;
  return GetImageForIndex(image_type, icon_type, connected_index);
}

gfx::ImageSkia GetImageForDisconnectedCellNetwork() {
  return GetDisconnectedImage(ICON_TYPE_LIST, shill::kTypeCellular);
}

gfx::ImageSkia GetImageForNewWifiNetwork(SkColor icon_color,
                                         SkColor badge_color) {
  SignalStrengthImageSource* source =
      new SignalStrengthImageSource(ImageTypeForNetworkType(shill::kTypeWifi),
                                    ICON_TYPE_LIST, kNumNetworkImages - 1);
  source->set_color(icon_color);
  gfx::ImageSkia icon = gfx::ImageSkia(source, source->size());
  Badges badges;
  badges.bottom_right =
      gfx::CreateVectorIcon(kNetworkBadgeAddOtherIcon, badge_color);
  return NetworkIconImageSource::CreateImage(icon, badges);
}

base::string16 GetLabelForNetwork(const chromeos::NetworkState* network,
                                  IconType icon_type) {
  DCHECK(network);
  std::string activation_state = network->activation_state();
  if (icon_type == ICON_TYPE_LIST || icon_type == ICON_TYPE_MENU_LIST) {
    // Show "<network>: [Connecting|Activating|Reconnecting]..."
    // TODO(varkha): Remaining states should migrate to secondary status in the
    // network item and no longer be part of the label.
    // See http://crbug.com/676181 .
    if (network->IsReconnecting()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_LIST_RECONNECTING,
          base::UTF8ToUTF16(network->name()));
    }
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
    // Show "[Connected to|Connecting to|Activating|Reconnecting to] <network>"
    // (non-list view).
    if (network->IsReconnecting()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_RECONNECTING,
          base::UTF8ToUTF16(network->name()));
    }
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

int GetCellularUninitializedMsg() {
  static base::Time s_uninitialized_state_time;
  static int s_uninitialized_msg(0);

  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  if (handler->GetTechnologyState(NetworkTypePattern::Mobile()) ==
      NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) {
    s_uninitialized_msg = IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    s_uninitialized_state_time = base::Time::Now();
    return s_uninitialized_msg;
  } else if (handler->GetScanningByType(NetworkTypePattern::Mobile())) {
    s_uninitialized_msg = IDS_ASH_STATUS_TRAY_MOBILE_SCANNING;
    s_uninitialized_state_time = base::Time::Now();
    return s_uninitialized_msg;
  }
  // There can be a delay between leaving the Initializing state and when
  // a Cellular device shows up, so keep showing the initializing
  // animation for a bit to avoid flashing the disconnect icon.
  const int kInitializingDelaySeconds = 1;
  base::TimeDelta dtime = base::Time::Now() - s_uninitialized_state_time;
  if (dtime.InSeconds() < kInitializingDelaySeconds)
    return s_uninitialized_msg;
  return 0;
}

void GetDefaultNetworkImageAndLabel(IconType icon_type,
                                    gfx::ImageSkia* image,
                                    base::string16* label,
                                    bool* animating) {
  NetworkStateHandler* state_handler =
      NetworkHandler::Get()->network_state_handler();
  NetworkConnectionHandler* connect_handler =
      NetworkHandler::Get()->network_connection_handler();
  const NetworkState* connected_network =
      state_handler->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());
  const NetworkState* connecting_network =
      state_handler->ConnectingNetworkByType(NetworkTypePattern::Wireless());
  if (!connecting_network && icon_type == ICON_TYPE_TRAY) {
    connecting_network =
        state_handler->ConnectingNetworkByType(NetworkTypePattern::VPN());
  }

  const NetworkState* network;
  // If we are connecting to a network, and there is either no connected
  // network, or the connection was user requested, or shill triggered a
  // reconnection, use the connecting network.
  if (connecting_network &&
      (!connected_network || connecting_network->IsReconnecting() ||
       connect_handler->HasConnectingNetwork(connecting_network->path()))) {
    network = connecting_network;
  } else {
    network = connected_network;
  }

  // Don't show ethernet in the tray
  if (icon_type == ICON_TYPE_TRAY && network &&
      network->Matches(NetworkTypePattern::Ethernet())) {
    *image = gfx::ImageSkia();
    *animating = false;
    return;
  }

  if (!network) {
    // If no connecting network, check if we are activating a network.
    const NetworkState* mobile_network =
        state_handler->FirstNetworkByType(NetworkTypePattern::Mobile());
    if (mobile_network && (mobile_network->activation_state() ==
                           shill::kActivationStateActivating)) {
      network = mobile_network;
    }
  }
  if (!network) {
    // If no connecting network, check for cellular initializing.
    int uninitialized_msg = GetCellularUninitializedMsg();
    if (uninitialized_msg != 0) {
      *image = GetConnectingImage(icon_type, shill::kTypeCellular);
      if (label)
        *label = l10n_util::GetStringUTF16(uninitialized_msg);
      *animating = true;
    } else {
      // Otherwise show the disconnected wifi icon.
      *image = GetDisconnectedImage(icon_type, shill::kTypeWifi);
      if (label) {
        *label = l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_NOT_CONNECTED);
      }
      *animating = false;
    }
    return;
  }
  *animating = network->IsConnectingState();
  // Get icon and label for connected or connecting network.
  *image = GetImageForNetwork(network, icon_type);
  if (label)
    *label = GetLabelForNetwork(network, icon_type);
}

void PurgeNetworkIconCache() {
  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkList(
      &networks);
  std::set<std::string> network_paths;
  for (NetworkStateHandler::NetworkStateList::iterator iter = networks.begin();
       iter != networks.end(); ++iter) {
    network_paths.insert((*iter)->path());
  }
  PurgeIconMap(ICON_TYPE_TRAY, network_paths);
  PurgeIconMap(ICON_TYPE_DEFAULT_VIEW, network_paths);
  PurgeIconMap(ICON_TYPE_LIST, network_paths);
  PurgeIconMap(ICON_TYPE_MENU_LIST, network_paths);
}

}  // namespace network_icon
}  // namespace ash
