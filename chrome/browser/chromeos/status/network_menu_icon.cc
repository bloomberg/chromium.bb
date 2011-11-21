// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_icon.h"

#include <algorithm>
#include <cmath>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/accessibility_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/skbitmap_operations.h"

using std::max;
using std::min;

namespace chromeos {

namespace {

// Amount to fade icons while connecting.
const double kConnectingImageAlpha = 0.5;

// Animation cycle length.
const int kThrobDurationMs = 750;

// Network strength bars images.
const int kNumBarsImages = 5;
SkBitmap kBarsImagesAnimating[kNumBarsImages - 1];

// Network strength arcs images.
const int kNumArcsImages = 5;
SkBitmap kArcsImagesAnimating[kNumArcsImages - 1];

// Badge offsets.  If a badge is large enough that it won't fit within the icon
// when using the right or bottom offset, it gets shifted inwards so it will.
const int kBadgeLeftX = 0;
const int kBadgeRightX = 14;
const int kBadgeTopY = 0;
const int kBadgeBottomY = 14;

// ID for VPN badge. TODO(stevenjb): replace with yellow lock icon.
const int kVpnBadgeId = IDR_STATUSBAR_NETWORK_SECURE;

int StrengthIndex(int strength, int count) {
  if (strength == 0) {
    return 0;
  } else {
    // Return an index in the range [1, count].
    float findex = (static_cast<float>(strength) / 100.0f) *
        nextafter(static_cast<float>(count), 0);
    int index = 1 + static_cast<int>(findex);
    index = max(min(index, count), 1);
    return index;
  }
}

int WifiStrengthIndex(const WifiNetwork* wifi) {
  return StrengthIndex(wifi->strength(), kNumArcsImages - 1);
}

int CellularStrengthIndex(const CellularNetwork* cellular) {
  if (cellular->data_left() == CellularNetwork::DATA_NONE)
    return 0;
  else
    return StrengthIndex(cellular->strength(), kNumBarsImages - 1);
}

const SkBitmap* BadgeForNetworkTechnology(const CellularNetwork* cellular) {
  int id = -1;
  switch (cellular->network_technology()) {
    case NETWORK_TECHNOLOGY_EVDO:
      switch (cellular->data_left()) {
        case CellularNetwork::DATA_NONE:
          id = IDR_STATUSBAR_NETWORK_3G_ERROR;
          break;
        case CellularNetwork::DATA_VERY_LOW:
        case CellularNetwork::DATA_LOW:
        case CellularNetwork::DATA_NORMAL:
          id = IDR_STATUSBAR_NETWORK_3G;
          break;
        case CellularNetwork::DATA_UNKNOWN:
          id = IDR_STATUSBAR_NETWORK_3G_UNKNOWN;
          break;
      }
      break;
    case NETWORK_TECHNOLOGY_1XRTT:
      switch (cellular->data_left()) {
        case CellularNetwork::DATA_NONE:
          id = IDR_STATUSBAR_NETWORK_1X_ERROR;
          break;
        case CellularNetwork::DATA_VERY_LOW:
        case CellularNetwork::DATA_LOW:
        case CellularNetwork::DATA_NORMAL:
          id = IDR_STATUSBAR_NETWORK_1X;
          break;
        case CellularNetwork::DATA_UNKNOWN:
          id = IDR_STATUSBAR_NETWORK_1X_UNKNOWN;
          break;
      }
      break;
      // Note: we may not be able to obtain data usage info from GSM carriers,
      // so there may not be a reason to create _ERROR or _UNKNOWN versions
      // of the following icons.
    case NETWORK_TECHNOLOGY_GPRS:
      id = IDR_STATUSBAR_NETWORK_GPRS;
      break;
    case NETWORK_TECHNOLOGY_EDGE:
      id = IDR_STATUSBAR_NETWORK_EDGE;
      break;
    case NETWORK_TECHNOLOGY_UMTS:
      id = IDR_STATUSBAR_NETWORK_3G;
      break;
    case NETWORK_TECHNOLOGY_HSPA:
      id = IDR_STATUSBAR_NETWORK_HSPA;
      break;
    case NETWORK_TECHNOLOGY_HSPA_PLUS:
      id = IDR_STATUSBAR_NETWORK_HSPA_PLUS;
      break;
    case NETWORK_TECHNOLOGY_LTE:
      id = IDR_STATUSBAR_NETWORK_LTE;
      break;
    case NETWORK_TECHNOLOGY_LTE_ADVANCED:
      id = IDR_STATUSBAR_NETWORK_LTE_ADVANCED;
      break;
    case NETWORK_TECHNOLOGY_GSM:
      id = IDR_STATUSBAR_NETWORK_GPRS;
      break;
    case NETWORK_TECHNOLOGY_UNKNOWN:
      break;
  }
  if (id == -1)
    return NULL;
  else
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(id);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetworkIcon

// Sets up and generates an SkBitmap for a Network icon.
class NetworkIcon {
 public:
  // Default constructor is used by the status bar icon (NetworkMenuIcon).
  NetworkIcon()
      : state_(STATE_UNKNOWN),
        strength_index_(-1),
        top_left_badge_(NULL),
        top_right_badge_(NULL),
        bottom_left_badge_(NULL),
        bottom_right_badge_(NULL),
        is_status_bar_(true),
        connected_network_(NULL),
        roaming_state_(ROAMING_STATE_UNKNOWN) {
  }

  // Service path constructor for cached network service icons.
  explicit NetworkIcon(const std::string& service_path)
      : service_path_(service_path),
        state_(STATE_UNKNOWN),
        strength_index_(-1),
        top_left_badge_(NULL),
        top_right_badge_(NULL),
        bottom_left_badge_(NULL),
        bottom_right_badge_(NULL),
        is_status_bar_(false),
        connected_network_(NULL),
        roaming_state_(ROAMING_STATE_UNKNOWN) {
  }

  ~NetworkIcon() {
  }

  void ClearIconAndBadges() {
    icon_ = SkBitmap();
    top_left_badge_ = NULL;
    top_right_badge_ = NULL;
    bottom_left_badge_ = NULL;
    bottom_right_badge_ = NULL;
  }

  void SetDirty() {
    state_ = STATE_UNKNOWN;
    strength_index_ = -1;
  }

  // Determines whether or not the associated network might be dirty and if so
  // updates and generates the icon. Does nothing if network no longer exists.
  void Update() {
    chromeos::NetworkLibrary* cros =
        chromeos::CrosLibrary::Get()->GetNetworkLibrary();
    // First look for a visible network.
    const Network* network = cros->FindNetworkByPath(service_path_);
    if (!network) {
      // If not a visible network, check for a remembered network.
      network = cros->FindRememberedNetworkByPath(service_path_);
      if (!network) {
        LOG(WARNING) << "Unable to find network:" << service_path_;
        return;
      }
    }
    bool dirty = bitmap_.empty();
    bool speak = false;
    if (accessibility::IsAccessibilityEnabled()) {
      if ((Network::IsConnectedState(state_) && !network->connected()) ||
          (Network::IsConnectingState(state_) && !network->connecting()) ||
          (Network::IsDisconnectedState(state_) && !network->disconnected())) {
        speak = true;
      }
    }
    if (state_ != network->state()) {
      state_ = network->state();
      dirty = true;
    }
    ConnectionType type = network->type();
    if (type == TYPE_WIFI || type == TYPE_CELLULAR) {
      int index = 0;
      if (type == TYPE_WIFI) {
        index = WifiStrengthIndex(
            static_cast<const WifiNetwork*>(network));
      } else if (type == TYPE_CELLULAR) {
        index = CellularStrengthIndex(
            static_cast<const CellularNetwork*>(network));
      }
      if (index != strength_index_) {
        strength_index_ = index;
        dirty = true;
      }
    }
    if (type == TYPE_CELLULAR) {
      const CellularNetwork* cellular =
          static_cast<const CellularNetwork*>(network);
      const SkBitmap* technology_badge = BadgeForNetworkTechnology(cellular);
      if (technology_badge != bottom_right_badge_)
        dirty = true;
      if (cellular->roaming_state() != roaming_state_) {
        roaming_state_ = cellular->roaming_state();
        dirty = true;
      }
    }
    if (type == TYPE_VPN) {
      if (cros->connected_network() != connected_network_) {
        connected_network_ = cros->connected_network();
        dirty = true;
      }
    }
    if (dirty) {
      UpdateIcon(network);
      GenerateBitmap();
    }
    if (speak) {
      std::string connection_string;
      if (Network::IsConnectedState(state_)) {
        switch (network->type()) {
          case TYPE_ETHERNET:
            connection_string = l10n_util::GetStringFUTF8(
                IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET));
            break;
          default:
            connection_string = l10n_util::GetStringFUTF8(
                IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
                UTF8ToUTF16(network->name()));
            break;
        }
      } else if (Network::IsConnectingState(state_)) {
        const Network* connecting_network = cros->connecting_network();
        if (connecting_network && connecting_network->type() != TYPE_ETHERNET) {
          connection_string = l10n_util::GetStringFUTF8(
              IDS_STATUSBAR_NETWORK_CONNECTING_TOOLTIP,
              UTF8ToUTF16(connecting_network->name()));
        }
      } else if (Network::IsDisconnectedState(state_)) {
        connection_string = l10n_util::GetStringUTF8(
            IDS_STATUSBAR_NETWORK_NO_NETWORK_TOOLTIP);
      }
      accessibility::Speak(connection_string.c_str());
    }
  }

  // Sets up the base icon image.
  void SetIcon(const Network* network) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    switch (network->type()) {
      case TYPE_ETHERNET: {
        icon_ = *rb.GetBitmapNamed(IDR_STATUSBAR_WIRED);
        break;
      }
      case TYPE_WIFI: {
        const WifiNetwork* wifi =
            static_cast<const WifiNetwork*>(network);
        if (strength_index_ == -1)
          strength_index_ = WifiStrengthIndex(wifi);
        icon_ = NetworkMenuIcon::GetBitmap(
            NetworkMenuIcon::ARCS, strength_index_);
        break;
      }
      case TYPE_CELLULAR: {
        const CellularNetwork* cellular =
            static_cast<const CellularNetwork*>(network);
        if (strength_index_ == -1)
          strength_index_ = CellularStrengthIndex(cellular);
        icon_ = NetworkMenuIcon::GetBitmap(
            NetworkMenuIcon::BARS, strength_index_);
        break;
      }
      default: {
        LOG(WARNING) << "Request for icon for unsupported type: "
                     << network->type();
        icon_ = *rb.GetBitmapNamed(IDR_STATUSBAR_WIRED);
        break;
      }
    }
  }

  // Sets up the various badges:
  // top_left: Cellular Roaming
  // top_right: libcros warning
  // bottom_left: VPN
  // bottom_right: disconnected / secure / technology / warning
  void SetBadges(const Network* network) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    chromeos::NetworkLibrary* cros =
        chromeos::CrosLibrary::Get()->GetNetworkLibrary();

    switch (network->type()) {
      case TYPE_ETHERNET: {
        if (network->disconnected()) {
          bottom_right_badge_ =
              rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED);
        }
        break;
      }
      case TYPE_WIFI: {
        const WifiNetwork* wifi =
            static_cast<const WifiNetwork*>(network);
        if (wifi->encrypted() && !is_status_bar_)
          bottom_right_badge_ = rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE);
        break;
      }
      case TYPE_CELLULAR: {
        const CellularNetwork* cellular =
            static_cast<const CellularNetwork*>(network);
        if (cellular->roaming_state() == ROAMING_STATE_ROAMING &&
            !cros->IsCellularAlwaysInRoaming()) {
          // For cellular that always in roaming don't show roaming badge.
          top_left_badge_ = rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_ROAMING);
        }
        if (!cellular->connecting())
          bottom_right_badge_ = BadgeForNetworkTechnology(cellular);
        break;
      }
      default:
        break;
    }
    // Display warning badge if cros is not loaded.
    if (is_status_bar_&& !CrosLibrary::Get()->load_error_string().empty())
      top_right_badge_ = rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_WARNING);
  }

  // Clears any previous state then sets the base icon and badges.
  void UpdateIcon(const Network* network) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    chromeos::NetworkLibrary* cros =
        chromeos::CrosLibrary::Get()->GetNetworkLibrary();

    ClearIconAndBadges();

    if (network->type() == TYPE_VPN) {
      // VPN should never be the primiary active network. This is used for
      // the icon next to a connected or disconnected VPN.
      const Network* connected_network = cros->connected_network();
      if (connected_network && connected_network->type() != TYPE_VPN) {
        // Set the icon and badges for the connected network.
        SetIcon(connected_network);
        SetBadges(connected_network);
      } else {
        // Use the ethernet icon for VPN when not connected.
        icon_ = *rb.GetBitmapNamed(IDR_STATUSBAR_WIRED);
        // We can be connected to a VPN, even when there is no connected
        // underlying network. In that case, for the status bar, show the
        // disconencted badge.
        if (is_status_bar_) {
          bottom_right_badge_ =
              rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED);
        }
      }
      // Overlay the VPN badge.
      bottom_left_badge_ = rb.GetBitmapNamed(kVpnBadgeId);
    } else {
      SetIcon(network);
      SetBadges(network);
    }
  }

  // Generates the bitmap. Call after setting the icon and badges.
  void GenerateBitmap() {
    if (icon_.empty())
      return;
    bitmap_ = NetworkMenuIcon::GenerateBitmapFromComponents(
        icon_,
        top_left_badge_,
        top_right_badge_,
        bottom_left_badge_,
        bottom_right_badge_);
  }

  const SkBitmap GetBitmap() const { return bitmap_; }

  void set_icon(const SkBitmap& icon) { icon_ = icon; }
  void set_top_left_badge(const SkBitmap* badge) {
    top_left_badge_ = badge;
  }
  void set_top_right_badge(const SkBitmap* badge) {
    top_right_badge_ = badge;
  }
  void set_bottom_left_badge(const SkBitmap* badge) {
    bottom_left_badge_ = badge;
  }
  void set_bottom_right_badge(const SkBitmap* badge) {
    bottom_right_badge_ = badge;
  }

 private:
  std::string service_path_;
  ConnectionState state_;
  int strength_index_;
  SkBitmap bitmap_;
  SkBitmap icon_;
  const SkBitmap* top_left_badge_;
  const SkBitmap* top_right_badge_;
  const SkBitmap* bottom_left_badge_;
  const SkBitmap* bottom_right_badge_;
  bool is_status_bar_;
  const Network* connected_network_;  // weak pointer; used for VPN icons.
  NetworkRoamingState roaming_state_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIcon);
};


////////////////////////////////////////////////////////////////////////////////
// NetworkMenuIcon

NetworkMenuIcon::NetworkMenuIcon(Delegate* delegate, Mode mode)
    : mode_(mode),
      delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_connecting_(this)),
      last_network_type_(TYPE_WIFI),
      connecting_network_(NULL) {
  // Generate empty images for blending.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const SkBitmap* vpn_badge = rb.GetBitmapNamed(kVpnBadgeId);
  empty_vpn_badge_.setConfig(SkBitmap::kARGB_8888_Config,
                             vpn_badge->width(), vpn_badge->height(), 0);
  empty_vpn_badge_.allocPixels();
  empty_vpn_badge_.eraseARGB(0, 0, 0, 0);

  // Set up the connection animation throbber.
  animation_connecting_.SetThrobDuration(kThrobDurationMs);
  animation_connecting_.SetTweenType(ui::Tween::LINEAR);

  // Initialize the icon.
  icon_.reset(new NetworkIcon());
}

NetworkMenuIcon::~NetworkMenuIcon() {
}

// Public methods:

const SkBitmap NetworkMenuIcon::GetIconAndText(string16* text) {
  SetIconAndText(text);
  icon_->GenerateBitmap();
  return icon_->GetBitmap();
}

// ui::AnimationDelegate:
void NetworkMenuIcon::AnimationProgressed(const ui::Animation* animation) {
  if (animation == &animation_connecting_ && delegate_) {
    // Only update the connecting network from here.
    if (GetConnectingNetwork() == connecting_network_)
      delegate_->NetworkMenuIconChanged();
  }
}

// Private methods:

// In menu mode, returns any connecting network.
// In dropdown mode, only returns connecting network if not connected.
const Network* NetworkMenuIcon::GetConnectingNetwork() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if ((mode_ == MENU_MODE) ||
      (mode_ == DROPDOWN_MODE && !cros->connected_network())) {
    const Network* connecting_network = cros->connecting_network();
    // Only show connecting icon for wireless networks.
    if (connecting_network && connecting_network->type() != TYPE_ETHERNET) {
      return connecting_network;
    }
  }
  return NULL;
}

// Starts the connection animation if necessary and returns its current value.
double NetworkMenuIcon::GetAnimation() {
  if (!animation_connecting_.is_animating()) {
    animation_connecting_.Reset();
    animation_connecting_.StartThrobbing(-1);
    return 0;
  }
  return animation_connecting_.GetCurrentValue();
}

// Fades the bars a bit and cycles through the different bar states.
void NetworkMenuIcon::SetConnectingIcon(const Network* network,
                                        double animation) {
  int image_count;
  BitmapType bitmap_type;
  SkBitmap* images;

  if (network->type() == TYPE_WIFI) {
    image_count = kNumArcsImages - 1;
    bitmap_type = ARCS;
    images = kArcsImagesAnimating;
  } else {
    image_count = kNumBarsImages - 1;
    bitmap_type = BARS;
    images = kBarsImagesAnimating;
  }
  int index = static_cast<int>(
      animation * nextafter(static_cast<float>(image_count), 0));
  index = max(min(index, image_count - 1), 0);

  // Lazily cache images.
  if (images[index].empty()) {
    SkBitmap source = GetBitmap(bitmap_type, index + 1);
    images[index] = NetworkMenuIcon::GenerateConnectingBitmap(source);
  }
  icon_->set_icon(images[index]);
}

// Sets up the icon and badges for GenerateBitmap().
void NetworkMenuIcon::SetIconAndText(string16* text) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  DCHECK(cros);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  icon_->ClearIconAndBadges();

  // If we are connecting to a network, display that.
  connecting_network_ = GetConnectingNetwork();
  if (connecting_network_) {
    double animation = GetAnimation();
    SetConnectingIcon(connecting_network_, animation);
    icon_->SetBadges(connecting_network_);
    if (text) {
      if (mode_ == MENU_MODE) {
        int tooltip_id = connecting_network_->configuring()
            ? IDS_STATUSBAR_NETWORK_CONFIGURING_TOOLTIP
            : IDS_STATUSBAR_NETWORK_CONNECTING_TOOLTIP;
        *text = l10n_util::GetStringFUTF16(
            tooltip_id, UTF8ToUTF16(connecting_network_->name()));
      } else {
        *text = UTF8ToUTF16(connecting_network_->name());
      }
    }
    return;
  }

  // If not connecting to a network, show the active or connected network.
  const Network* network;
  if (mode_ == DROPDOWN_MODE && cros->connected_network())
    network = cros->connected_network();
  else
    network = cros->active_network();
  if (network) {
    bool animating = false;
    last_network_type_ = network->type();
    // Icon + badges.
    icon_->SetDirty();
    icon_->UpdateIcon(network);
    // Overlay the VPN badge if connecting to a VPN.
    if (network->type() != TYPE_VPN && cros->virtual_network()) {
      if (cros->virtual_network()->connecting()) {
        const SkBitmap* vpn_badge = rb.GetBitmapNamed(kVpnBadgeId);
        double animation = GetAnimation();
        animating = true;
        // Even though this is the only place we use vpn_connecting_badge_,
        // it is important that this is a member variable since we set a
        // pointer to it and access that pointer in icon_->GenerateBitmap().
        vpn_connecting_badge_ = SkBitmapOperations::CreateBlendedBitmap(
            empty_vpn_badge_, *vpn_badge, animation);
        icon_->set_bottom_left_badge(&vpn_connecting_badge_);
      }
    }
    if (!animating)
      animation_connecting_.Stop();
    // Text.
    if (text) {
      switch (network->type()) {
        case TYPE_ETHERNET:
          if (mode_ == MENU_MODE) {
            *text = l10n_util::GetStringFUTF16(
                IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET));
          } else {
            *text = l10n_util::GetStringUTF16(
                IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
          }
          break;
        default:
          if (mode_ == MENU_MODE) {
            *text = l10n_util::GetStringFUTF16(
                IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
                UTF8ToUTF16(network->name()));
          } else {
            *text = UTF8ToUTF16(network->name());
          }
          break;
      }
    }
    return;
  }

  // Not connecting, so stop animation.
  animation_connecting_.Stop();

  // No connecting, connected, or active network.
  switch (last_network_type_) {
    case TYPE_ETHERNET:
      icon_->set_icon(*rb.GetBitmapNamed(IDR_STATUSBAR_WIRED));
      icon_->set_bottom_right_badge(
          rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED));
      break;
    case TYPE_WIFI:
      icon_->set_icon(GetDisconnectedBitmap(ARCS));
      break;
    case TYPE_CELLULAR:
    default:
      icon_->set_icon(GetDisconnectedBitmap(BARS));
      icon_->set_bottom_right_badge(
          rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED));
      break;
  }
  if (text) {
    if (mode_ == MENU_MODE) {
      *text =  l10n_util::GetStringUTF16(
          IDS_STATUSBAR_NETWORK_NO_NETWORK_TOOLTIP);
    } else {
      *text = l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_NONE);  }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Static functions for generating network icon bitmaps:

// This defines how we assemble a network icon.
const SkBitmap NetworkMenuIcon::GenerateBitmapFromComponents(
    const SkBitmap& icon,
    const SkBitmap* top_left_badge,
    const SkBitmap* top_right_badge,
    const SkBitmap* bottom_left_badge,
    const SkBitmap* bottom_right_badge) {
  DCHECK(!icon.empty());
  gfx::CanvasSkia canvas(icon.width(), icon.height(), false);
  canvas.DrawBitmapInt(icon, 0, 0);

  if (top_left_badge) {
    canvas.DrawBitmapInt(*top_left_badge, kBadgeLeftX, kBadgeTopY);
  }
  if (top_right_badge) {
    int x = min(kBadgeRightX, icon.width() - top_right_badge->width());
    canvas.DrawBitmapInt(*top_right_badge, x, kBadgeTopY);
  }
  if (bottom_left_badge) {
    int y = min(kBadgeBottomY, icon.height() - bottom_left_badge->height());
    canvas.DrawBitmapInt(*bottom_left_badge, kBadgeLeftX, y);
  }
  if (bottom_right_badge) {
    int x = min(kBadgeRightX, icon.width() - bottom_right_badge->width());
    int y = min(kBadgeBottomY, icon.height() - bottom_right_badge->height());
    canvas.DrawBitmapInt(*bottom_right_badge, x, y);
  }

  return canvas.ExtractBitmap();
}

// We blend connecting icons with a black image to generate a faded icon.
const SkBitmap NetworkMenuIcon::GenerateConnectingBitmap(
    const SkBitmap& source) {
  CR_DEFINE_STATIC_LOCAL(SkBitmap, empty_badge, ());
  if (empty_badge.empty()) {
    empty_badge.setConfig(SkBitmap::kARGB_8888_Config,
                          source.width(), source.height(), 0);
    empty_badge.allocPixels();
    empty_badge.eraseARGB(0, 0, 0, 0);
  }
  DCHECK(empty_badge.width() == source.width());
  DCHECK(empty_badge.height() == source.height());
  return SkBitmapOperations::CreateBlendedBitmap(
      empty_badge, source, kConnectingImageAlpha);
}

// Generates and caches an icon bitmap for a network's current state.
const SkBitmap NetworkMenuIcon::GetBitmap(const Network* network) {
  DCHECK(network);
  // Maintain a static (global) icon map. Note: Icons are never destroyed;
  // it is assumed that a finite and reasonable number of network icons will be
  // created during a session.
  typedef std::map<std::string, NetworkIcon*> NetworkIconMap;
  static NetworkIconMap* icon_map = NULL;
  if (icon_map == NULL)
    icon_map = new NetworkIconMap;
  // Find or add the icon.
  NetworkIcon* icon;
  NetworkIconMap::iterator iter = icon_map->find(network->service_path());
  if (iter == icon_map->end()) {
    icon = new NetworkIcon(network->service_path());
    icon_map->insert(std::make_pair(network->service_path(), icon));
  } else {
    icon = iter->second;
  }
  // Update and return the icon's bitmap.
  icon->Update();
  return icon->GetBitmap();
}

// Returns an icon for a disconnected VPN.
const SkBitmap NetworkMenuIcon::GetVpnBitmap() {
  static SkBitmap* vpn_bitmap = NULL;
  if (vpn_bitmap == NULL) {
    // Set the disconencted vpn icon (ethernet + VPN) for GetVpnIcon().
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const SkBitmap* ethernet_icon = rb.GetBitmapNamed(IDR_STATUSBAR_WIRED);
    gfx::CanvasSkia canvas(
        ethernet_icon->width(), ethernet_icon->height(), false);
    canvas.DrawBitmapInt(*ethernet_icon, 0, 0);
    const SkBitmap* vpn_badge = rb.GetBitmapNamed(kVpnBadgeId);
    canvas.DrawBitmapInt(*vpn_badge, kBadgeLeftX, kBadgeBottomY);
    vpn_bitmap = new SkBitmap(canvas.ExtractBitmap());
  }
  return *vpn_bitmap;
}

const SkBitmap NetworkMenuIcon::GetBitmap(BitmapType type, int index) {
  static int arcs_width = 0, arcs_height = 0, bars_width = 0, bars_height = 0;
  int width, height;
  SkBitmap* images;
  if (type == ARCS) {
    if (index >= kNumArcsImages)
      return SkBitmap();

    images = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_STATUSBAR_NETWORK_ARCS);
    if (arcs_width == 0)
      arcs_width = images->width();
    if (arcs_height == 0)
      arcs_height = images->height() / kNumArcsImages;
    width = arcs_width;
    height = arcs_height;
  } else {
    if (index >= kNumBarsImages)
      return SkBitmap();

    images = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_STATUSBAR_NETWORK_BARS);
    if (bars_width == 0)
      bars_width = images->width();
    if (bars_height == 0)
      bars_height = images->height() / kNumBarsImages;
    width = bars_width;
    height = bars_height;
  }

  SkIRect subset = SkIRect::MakeXYWH(0, index * height, width, height);
  SkBitmap image;
  images->extractSubset(&image, subset);
  return image;
}

const SkBitmap NetworkMenuIcon::GetDisconnectedBitmap(BitmapType type) {
  return GetBitmap(type, 0);
}

const SkBitmap NetworkMenuIcon::GetConnectedBitmap(BitmapType type) {
  return GetBitmap(type, NumBitmaps(type) - 1);
}

int NetworkMenuIcon::NumBitmaps(BitmapType type) {
  return (type == ARCS) ? kNumArcsImages : kNumBarsImages;
}

}  // chromeos
