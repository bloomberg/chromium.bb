// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_icon.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

#include "ash/system/chromeos/network/network_icon_animation.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/size_conversions.h"

using std::max;
using std::min;

using ash::network_icon::NetworkIconAnimation;

namespace chromeos {

namespace {

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

// Badge offsets. The right and bottom offsets are computed based on the size
// of the network icon and the badge in order to accomodate multiple icon
// resolutions (ie. standard and high DPI).
const int kBadgeLeftX = 0;
const int kBadgeTopY = 0;

int StrengthIndex(int strength, int count) {
  if (strength == 0) {
    return 0;
  } else {
    // Return an index in the range [1, count].
    const float findex = (static_cast<float>(strength) / 100.0f) *
        nextafter(static_cast<float>(count), 0);
    int index = 1 + static_cast<int>(findex);
    index = max(min(index, count), 1);
    return index;
  }
}

int WifiStrengthIndex(const WifiNetwork* wifi) {
  return StrengthIndex(wifi->strength(), kNumArcsImages - 1);
}

int WimaxStrengthIndex(const WimaxNetwork* wimax) {
  return StrengthIndex(wimax->strength(), kNumBarsImages - 1);
}

int CellularStrengthIndex(const CellularNetwork* cellular) {
  return StrengthIndex(cellular->strength(), kNumBarsImages - 1);
}

const gfx::ImageSkia* BadgeForNetworkTechnology(
    const CellularNetwork* cellular,
    NetworkMenuIcon::ResourceColorTheme color) {
  const int kUnknownBadgeType = -1;
  int id = kUnknownBadgeType;
  switch (cellular->network_technology()) {
    case NETWORK_TECHNOLOGY_EVDO:
      id = (color == NetworkMenuIcon::COLOR_DARK) ?
          IDR_AURA_UBER_TRAY_NETWORK_3G_DARK :
          IDR_AURA_UBER_TRAY_NETWORK_3G_LIGHT;
      break;
    case NETWORK_TECHNOLOGY_1XRTT:
      id = IDR_AURA_UBER_TRAY_NETWORK_1X;
      break;
    case NETWORK_TECHNOLOGY_GPRS:
      id = (color == NetworkMenuIcon::COLOR_DARK) ?
          IDR_AURA_UBER_TRAY_NETWORK_GPRS_DARK :
          IDR_AURA_UBER_TRAY_NETWORK_GPRS_LIGHT;
      break;
    case NETWORK_TECHNOLOGY_EDGE:
      id = (color == NetworkMenuIcon::COLOR_DARK) ?
          IDR_AURA_UBER_TRAY_NETWORK_EDGE_DARK :
          IDR_AURA_UBER_TRAY_NETWORK_EDGE_LIGHT;
      break;
    case NETWORK_TECHNOLOGY_UMTS:
      id = (color == NetworkMenuIcon::COLOR_DARK) ?
          IDR_AURA_UBER_TRAY_NETWORK_3G_DARK :
          IDR_AURA_UBER_TRAY_NETWORK_3G_LIGHT;
      break;
    case NETWORK_TECHNOLOGY_HSPA:
      id = (color == NetworkMenuIcon::COLOR_DARK) ?
          IDR_AURA_UBER_TRAY_NETWORK_HSPA_DARK :
          IDR_AURA_UBER_TRAY_NETWORK_HSPA_LIGHT;
      break;
    case NETWORK_TECHNOLOGY_HSPA_PLUS:
      id = (color == NetworkMenuIcon::COLOR_DARK) ?
          IDR_AURA_UBER_TRAY_NETWORK_HSPA_PLUS_DARK :
          IDR_AURA_UBER_TRAY_NETWORK_HSPA_PLUS_LIGHT;
      break;
    case NETWORK_TECHNOLOGY_LTE:
      id = (color == NetworkMenuIcon::COLOR_DARK) ?
          IDR_AURA_UBER_TRAY_NETWORK_LTE_DARK :
          IDR_AURA_UBER_TRAY_NETWORK_LTE_LIGHT;
      break;
    case NETWORK_TECHNOLOGY_LTE_ADVANCED:
      id = (color == NetworkMenuIcon::COLOR_DARK) ?
          IDR_AURA_UBER_TRAY_NETWORK_LTE_ADVANCED_DARK :
          IDR_AURA_UBER_TRAY_NETWORK_LTE_ADVANCED_LIGHT;
      break;
    case NETWORK_TECHNOLOGY_GSM:
      id = (color == NetworkMenuIcon::COLOR_DARK) ?
          IDR_AURA_UBER_TRAY_NETWORK_GPRS_DARK :
          IDR_AURA_UBER_TRAY_NETWORK_GPRS_LIGHT;
      break;
    case NETWORK_TECHNOLOGY_UNKNOWN:
      break;
  }
  if (id == kUnknownBadgeType)
    return NULL;
  else
    return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

const SkBitmap GetEmptyBitmap(const gfx::Size pixel_size) {
  typedef std::pair<int, int> SizeKey;
  typedef std::map<SizeKey, SkBitmap> SizeBitmapMap;
  static SizeBitmapMap* empty_bitmaps_ = new SizeBitmapMap;

  SizeKey key(pixel_size.width(), pixel_size.height());

  SizeBitmapMap::iterator iter = empty_bitmaps_->find(key);
  if (iter != empty_bitmaps_->end())
    return iter->second;

  SkBitmap empty;
  empty.setConfig(SkBitmap::kARGB_8888_Config, key.first, key.second);
  empty.allocPixels();
  empty.eraseARGB(0, 0, 0, 0);
  (*empty_bitmaps_)[key] = empty;
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
  NetworkIconImageSource(const gfx::ImageSkia& icon,
                         const gfx::ImageSkia* top_left_badge,
                         const gfx::ImageSkia* top_right_badge,
                         const gfx::ImageSkia* bottom_left_badge,
                         const gfx::ImageSkia* bottom_right_badge)
      : icon_(icon),
        top_left_badge_(top_left_badge),
        top_right_badge_(top_right_badge),
        bottom_left_badge_(bottom_left_badge),
        bottom_right_badge_(bottom_right_badge) {
  }
  virtual ~NetworkIconImageSource() {}

  virtual gfx::ImageSkiaRep GetImageForScale(
      ui::ScaleFactor scale_factor) OVERRIDE {
    gfx::ImageSkiaRep icon_rep = icon_.GetRepresentation(scale_factor);
    if (icon_rep.is_null())
      return gfx::ImageSkiaRep();
    gfx::Canvas canvas(icon_rep, false);
    if (top_left_badge_)
      canvas.DrawImageInt(*top_left_badge_, kBadgeLeftX, kBadgeTopY);
    if (top_right_badge_)
      canvas.DrawImageInt(*top_right_badge_,
                          icon_.width() - top_right_badge_->width(),
                          kBadgeTopY);
    if (bottom_left_badge_) {
      canvas.DrawImageInt(*bottom_left_badge_,
                          kBadgeLeftX,
                          icon_.height() - bottom_left_badge_->height());
    }
    if (bottom_right_badge_) {
      canvas.DrawImageInt(*bottom_right_badge_,
                          icon_.width() - bottom_right_badge_->width(),
                          icon_.height() - bottom_right_badge_->height());
    }
    return canvas.ExtractImageRep();
  }

 private:
  const gfx::ImageSkia icon_;
  const gfx::ImageSkia *top_left_badge_;
  const gfx::ImageSkia *top_right_badge_;
  const gfx::ImageSkia *bottom_left_badge_;
  const gfx::ImageSkia *bottom_right_badge_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIconImageSource);
};

gfx::ImageSkia GetEmptyImage(const gfx::Size& size) {
  return gfx::ImageSkia(new EmptyImageSource(size), size);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetworkIcon

// Sets up and generates an ImageSkia for a Network icon.
class NetworkIcon {
 public:
  // Default constructor is used by the status bar icon (NetworkMenuIcon).
  explicit NetworkIcon(NetworkMenuIcon::ResourceColorTheme color);

  // Service path constructor for cached network service icons.
  NetworkIcon(const std::string& service_path,
              NetworkMenuIcon::ResourceColorTheme color);

  ~NetworkIcon();

  // Resets the icon state.
  void ClearIconAndBadges();

  // Resets the saved state to force an update.
  void SetDirty();

  // Updates |vpn_connected_|, returns true if it changed.
  bool SetOrClearVpnConnected(const Network* network);

  // Determines whether or not the associated network might be dirty and if so
  // updates and generates the icon. Does nothing if network no longer exists.
  void Update();

  // Sets up the base icon image.
  void SetIcon(const Network* network);

  // Sets up the various badges:
  // top_left: cellular roaming
  // top_right: libcros warning
  // bottom_left: VPN
  // bottom_right: disconnected / secure / technology / warning
  void SetBadges(const Network* network);

  // Clears any previous state then sets the base icon and badges.
  void UpdateIcon(const Network* network);

  // Generates the image. Call after setting the icon and badges.
  void GenerateImage();

  const gfx::ImageSkia GetImage() const { return image_; }

  bool ShouldShowInTray() const;

  ConnectionType type() { return type_; }
  void set_type(ConnectionType type) { type_ = type; }
  void set_state(ConnectionState state) { state_ = state; }
  void set_icon(const gfx::ImageSkia& icon) { icon_ = icon; }
  void set_top_left_badge(const gfx::ImageSkia* badge) {
    top_left_badge_ = badge;
  }
  void set_top_right_badge(const gfx::ImageSkia* badge) {
    top_right_badge_ = badge;
  }
  void set_bottom_left_badge(const gfx::ImageSkia* badge) {
    bottom_left_badge_ = badge;
  }
  void set_bottom_right_badge(const gfx::ImageSkia* badge) {
    bottom_right_badge_ = badge;
  }

 private:
  // Updates strength_index_ for wifi or cellular networks.
  // Returns true if |strength_index_| changed.
  bool UpdateWirelessStrengthIndex(const Network* network);

  // Updates the local state for cellular networks.
  bool UpdateCellularState(const Network* network);

  std::string service_path_;
  ConnectionType type_;
  ConnectionState state_;
  NetworkMenuIcon::ResourceColorTheme resource_color_theme_;
  int strength_index_;
  gfx::ImageSkia image_;
  gfx::ImageSkia icon_;
  const gfx::ImageSkia* top_left_badge_;
  const gfx::ImageSkia* top_right_badge_;
  const gfx::ImageSkia* bottom_left_badge_;
  const gfx::ImageSkia* bottom_right_badge_;
  bool is_status_bar_;
  const Network* connected_network_;  // weak pointer; used for VPN icons.
  bool vpn_connected_;
  NetworkRoamingState roaming_state_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIcon);
};

////////////////////////////////////////////////////////////////////////////////
// NetworkIcon

NetworkIcon::NetworkIcon(NetworkMenuIcon::ResourceColorTheme color)
    : type_(TYPE_UNKNOWN),
      state_(STATE_UNKNOWN),
      resource_color_theme_(color),
      strength_index_(-1),
      top_left_badge_(NULL),
      top_right_badge_(NULL),
      bottom_left_badge_(NULL),
      bottom_right_badge_(NULL),
      is_status_bar_(true),
      connected_network_(NULL),
      vpn_connected_(false),
      roaming_state_(ROAMING_STATE_UNKNOWN) {
}

NetworkIcon::NetworkIcon(const std::string& service_path,
                         NetworkMenuIcon::ResourceColorTheme color)
    : service_path_(service_path),
      type_(TYPE_UNKNOWN),
      state_(STATE_UNKNOWN),
      resource_color_theme_(color),
      strength_index_(-1),
      top_left_badge_(NULL),
      top_right_badge_(NULL),
      bottom_left_badge_(NULL),
      bottom_right_badge_(NULL),
      is_status_bar_(false),
      connected_network_(NULL),
      vpn_connected_(false),
      roaming_state_(ROAMING_STATE_UNKNOWN) {
}

NetworkIcon::~NetworkIcon() {
}

void NetworkIcon::ClearIconAndBadges() {
  icon_ = gfx::ImageSkia();
  top_left_badge_ = NULL;
  top_right_badge_ = NULL;
  bottom_left_badge_ = NULL;
  bottom_right_badge_ = NULL;
}

void NetworkIcon::SetDirty() {
  state_ = STATE_UNKNOWN;
  strength_index_ = -1;
}

bool NetworkIcon::SetOrClearVpnConnected(const Network* network) {
  if (network->type() == TYPE_VPN)
    return false;  // Never show the VPN badge for a VPN network.
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  bool vpn_connected = (network->connected() &&
                        cros->virtual_network() &&
                        cros->virtual_network()->connected());
  if (vpn_connected_ != vpn_connected) {
    vpn_connected_ = vpn_connected;
    return true;
  }
  return false;
}

void NetworkIcon::Update() {
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

  // Determine whether or not we need to update the icon.
  bool dirty = image_.isNull();

  // If the network state has changed, the icon needs updating.
  if (state_ != network->state()) {
    state_ = network->state();
    dirty = true;
  }

  type_ = network->type();

  if (type_ == TYPE_WIFI || type_ == TYPE_WIMAX || type_ == TYPE_CELLULAR) {
    if (UpdateWirelessStrengthIndex(network))
      dirty = true;
  }

  if (type_ == TYPE_CELLULAR) {
    if (UpdateCellularState(network))
      dirty = true;
  }

  if (type_ == TYPE_VPN) {
    // For VPN, check to see if the connected network has changed.
    if (cros->connected_network() != connected_network_) {
      connected_network_ = cros->connected_network();
      dirty = true;
    }
  }

  if (dirty) {
    // Set the icon and badges based on the network.
    UpdateIcon(network);
    // Generate the image from the icon.
    GenerateImage();
  }
}

void NetworkIcon::SetIcon(const Network* network) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  set_type(network->type());
  set_state(network->state());

  switch (type_) {
    case TYPE_ETHERNET: {
      icon_ = *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_WIRED);
      break;
    }
    case TYPE_WIFI: {
      const WifiNetwork* wifi = static_cast<const WifiNetwork*>(network);
      if (strength_index_ == -1)
        strength_index_ = WifiStrengthIndex(wifi);
      icon_ = NetworkMenuIcon::GetImage(
          NetworkMenuIcon::ARCS, strength_index_, resource_color_theme_);
      break;
    }
    case TYPE_WIMAX: {
      const WimaxNetwork* wimax = static_cast<const WimaxNetwork*>(network);
      if (strength_index_ == -1)
        strength_index_ =  WimaxStrengthIndex(wimax);
      icon_ = NetworkMenuIcon::GetImage(
          NetworkMenuIcon::BARS, strength_index_, resource_color_theme_);
      break;
    }
    case TYPE_CELLULAR: {
      const CellularNetwork* cellular =
          static_cast<const CellularNetwork*>(network);
      if (strength_index_ == -1)
        strength_index_ = CellularStrengthIndex(cellular);
      icon_ = NetworkMenuIcon::GetImage(
          NetworkMenuIcon::BARS, strength_index_, resource_color_theme_);
      break;
    }
    case TYPE_VPN: {
      icon_ = *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_VPN);
      break;
    }
    default: {
      LOG(WARNING) << "Request for icon for unsupported type: " << type_;
      icon_ = *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_WIRED);
      break;
    }
  }
}

void NetworkIcon::SetBadges(const Network* network) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();

  bool use_dark_icons = resource_color_theme_ == NetworkMenuIcon::COLOR_DARK;
  switch (network->type()) {
    case TYPE_ETHERNET:
      break;
    case TYPE_WIFI: {
      const WifiNetwork* wifi = static_cast<const WifiNetwork*>(network);
      if (wifi->encrypted() && use_dark_icons) {
        bottom_right_badge_ = rb.GetImageSkiaNamed(
            IDR_AURA_UBER_TRAY_NETWORK_SECURE_DARK);
      }
      break;
    }
    case TYPE_WIMAX: {
      top_left_badge_ = rb.GetImageSkiaNamed(
          use_dark_icons ?
          IDR_AURA_UBER_TRAY_NETWORK_4G_DARK :
          IDR_AURA_UBER_TRAY_NETWORK_4G_LIGHT);
      break;
    }
    case TYPE_CELLULAR: {
      const CellularNetwork* cellular =
            static_cast<const CellularNetwork*>(network);
      if (cellular->roaming_state() == ROAMING_STATE_ROAMING &&
          !cros->IsCellularAlwaysInRoaming()) {
        // For cellular that always in roaming don't show roaming badge.
        bottom_right_badge_ = rb.GetImageSkiaNamed(use_dark_icons ?
            IDR_AURA_UBER_TRAY_NETWORK_ROAMING_DARK :
            IDR_AURA_UBER_TRAY_NETWORK_ROAMING_LIGHT);
      }
      if (!cellular->connecting()) {
        top_left_badge_ = BadgeForNetworkTechnology(cellular,
                                                    resource_color_theme_);
      }
      break;
    }
    default:
      break;
  }
  if (vpn_connected_ && network->type() != TYPE_VPN) {
    bottom_left_badge_ = rb.GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NETWORK_VPN_BADGE);
  }
}

void NetworkIcon::UpdateIcon(const Network* network) {
  ClearIconAndBadges();
  SetIcon(network);
  SetBadges(network);
}

void NetworkIcon::GenerateImage() {
  if (icon_.isNull()) {
    set_icon(NetworkMenuIcon::GetDisconnectedImage(
        NetworkMenuIcon::ARCS, resource_color_theme_));
  }
  image_ = NetworkMenuIcon::GenerateImageFromComponents(icon_, top_left_badge_,
      top_right_badge_, bottom_left_badge_, bottom_right_badge_);
}

bool NetworkIcon::ShouldShowInTray() const {
  if (type_ != TYPE_ETHERNET)
    return true;
  if (!Network::IsConnectedState(state_))
    return true;
  NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
  if (crosnet->virtual_network())
    return true;
  return false;
}

bool NetworkIcon::UpdateWirelessStrengthIndex(const Network* network) {
  bool dirty = false;
  ConnectionType type = network->type();
  int index = 0;
  if (type == TYPE_WIFI) {
    index = WifiStrengthIndex(static_cast<const WifiNetwork*>(network));
  } else if (type == TYPE_WIMAX) {
    index = WimaxStrengthIndex(static_cast<const WimaxNetwork*>(network));
  } else if (type == TYPE_CELLULAR) {
    index = CellularStrengthIndex(static_cast<const CellularNetwork*>(network));
  }
  if (index != strength_index_) {
    strength_index_ = index;
    dirty = true;
  }
  return dirty;
}

bool NetworkIcon::UpdateCellularState(const Network* network) {
  if (network->type() != TYPE_CELLULAR)
    return false;
  bool dirty = false;
  const CellularNetwork* cellular =
    static_cast<const CellularNetwork*>(network);
  const gfx::ImageSkia* technology_badge = BadgeForNetworkTechnology(
      cellular, resource_color_theme_);
  if (technology_badge != top_left_badge_) {
    dirty = true;
  }
  if (cellular->roaming_state() != roaming_state_) {
    roaming_state_ = cellular->roaming_state();
    dirty = true;
  }
  return dirty;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuIcon

NetworkMenuIcon::NetworkMenuIcon(Delegate* delegate, Mode mode)
    : mode_(mode),
      delegate_(delegate),
      resource_color_theme_(COLOR_DARK),
      connecting_index_(-1) {
  // Initialize the icon.
  icon_.reset(new NetworkIcon(resource_color_theme_));
}

NetworkMenuIcon::~NetworkMenuIcon() {
  // Remove itself from NetworkIconAnimation's observer list just in case
  // it has been added before and not been removed yet.
  NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

// Public methods:

void NetworkMenuIcon::SetResourceColorTheme(ResourceColorTheme color) {
  if (color == resource_color_theme_)
    return;

  resource_color_theme_ = color;
  icon_.reset(new NetworkIcon(resource_color_theme_));
}

bool NetworkMenuIcon::ShouldShowIconInTray() {
  if (!icon_.get())
    return false;
  return icon_->ShouldShowInTray();
}

const gfx::ImageSkia NetworkMenuIcon::GetIconAndText(string16* text) {
  if (SetIconAndText())
    NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  if (text)
    *text = text_;
  icon_->GenerateImage();
  return icon_->GetImage();
}

const gfx::ImageSkia NetworkMenuIcon::GetVpnIconAndText(string16* text) {
  if (SetVpnIconAndText())
    NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  if (text)
    *text = text_;
  icon_->GenerateImage();
  return icon_->GetImage();
}

void NetworkMenuIcon::NetworkIconChanged() {
  if (!delegate_ || !icon_.get())
    return;
  // Only send a message when the icon would change.
  int connecting_index = GetConnectingIndex();
  if (connecting_index != connecting_index_) {
    connecting_index_ = connecting_index;
    delegate_->NetworkMenuIconChanged();
  }
}

// Private methods:

// If disconnected: returns any connecting non-ethernet network.
// Otherwise, only return a network if the conenction was user initiated.
const Network* NetworkMenuIcon::GetConnectingNetwork() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  const Network* connecting_network = cros->connecting_network();
  if (connecting_network &&
      connecting_network->type() != TYPE_ETHERNET &&
      (!cros->connected_network() ||
       connecting_network->connection_started())) {
    return connecting_network;
  }
  return NULL;
}

double NetworkMenuIcon::GetAnimation() {
  return NetworkIconAnimation::GetInstance()->GetAnimation();
}

int NetworkMenuIcon::GetConnectingIndex() {
  DCHECK(icon_.get());
  double animation = GetAnimation();
  int image_count =
      (icon_->type() == TYPE_WIFI) ? kNumArcsImages - 1 : kNumBarsImages - 1;
  int index = animation * nextafter(static_cast<float>(image_count), 0);
  return std::max(std::min(index, image_count - 1), 0);
}


// TODO(stevenjb): move below SetIconAndText.
void NetworkMenuIcon::SetConnectingIconAndText(
    const Network* connecting_network) {
  connecting_network_ = connecting_network;
  ConnectionType type;
  ConnectionState state;
  if (connecting_network_) {
    type = connecting_network_->type();
    state = connecting_network_->state();
    if (mode_ == MENU_MODE) {
      text_ = l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_CONNECTING_TOOLTIP,
          UTF8ToUTF16(connecting_network_->name()));
    } else {
      text_ = UTF8ToUTF16(connecting_network_->name());
    }
  } else {
    // When called with no connecting network, cellular is initializing.
    type = TYPE_CELLULAR;
    state = STATE_ASSOCIATION;
    text_ = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR);
  }
  icon_->set_type(type);
  icon_->set_state(state);

  ImageType image_type;
  gfx::ImageSkia** images;
  if (type == TYPE_WIFI) {
    image_type = ARCS;
    images = resource_color_theme_ == COLOR_DARK ? kArcsImagesAnimatingDark :
                                                   kArcsImagesAnimatingLight;
  } else {
    image_type = BARS;
    images = resource_color_theme_ == COLOR_DARK ? kBarsImagesAnimatingDark :
                                                   kBarsImagesAnimatingLight;
  }
  int index = GetConnectingIndex();

  // Lazily cache images.
  if (!images[index]) {
    gfx::ImageSkia source =
        GetImage(image_type, index + 1, resource_color_theme_);
    images[index] =
        new gfx::ImageSkia(NetworkMenuIcon::GenerateConnectingImage(source));
  }
  icon_->set_icon(*images[index]);
  if (connecting_network_)
    icon_->SetBadges(connecting_network_);
}

// Sets up the icon and badges for GenerateBitmap().
bool NetworkMenuIcon::SetIconAndText() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  DCHECK(cros);

  icon_->ClearIconAndBadges();

  // If we are connecting to a network and it was user-initiated or we are
  // not connected, display that.
  const Network* connecting_network = GetConnectingNetwork();
  if (connecting_network) {
    SetConnectingIconAndText(connecting_network);
    return true;
  }

  // If not connecting to a network, show the active or connected network.
  const Network* network;
  if (mode_ == DROPDOWN_MODE && cros->connected_network())
    network = cros->connected_network();
  else
    network = cros->active_nonvirtual_network();
  if (network)
    return SetActiveNetworkIconAndText(network);

  // If no connected network, check if we are initializing Cellular.
  if (mode_ != DROPDOWN_MODE && cros->cellular_initializing()) {
    initialize_state_time_ = base::Time::Now();
    SetConnectingIconAndText(NULL);
    return true;
  }
  // There can be a delay between leaving the Initializing state and when a
  // Cellular device shows up, so keep showing the initializing animation
  // for a few extra seconds to avoid flashing the disconnect icon.
  const int kInitializingDelaySeconds = 1;
  base::TimeDelta dtime = base::Time::Now() - initialize_state_time_;
  if (dtime.InSeconds() < kInitializingDelaySeconds) {
    SetConnectingIconAndText(NULL);
    return true;
  }

  // No connecting, connected, or active network.
  SetDisconnectedIconAndText();
  return false;
}

bool NetworkMenuIcon::SetVpnIconAndText() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  DCHECK(cros);

  icon_->ClearIconAndBadges();
  const VirtualNetwork* vpn = cros->virtual_network();
  if (!vpn) {
    LOG(WARNING) << "SetVpnIconAndText called with no VPN";
    SetDisconnectedIconAndText();
    return false;
  }
  if (vpn->connecting()) {
    SetConnectingIconAndText(vpn);
    return true;
  }

  // If not connecting to a VPN, show the active/connected VPN.
  return SetActiveNetworkIconAndText(vpn);
}

bool NetworkMenuIcon::SetActiveNetworkIconAndText(const Network* network) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  bool animating = false;

  // Set icon and badges. Call SetDirty() since network may have changed.
  icon_->SetDirty();
  icon_->SetOrClearVpnConnected(network);
  icon_->UpdateIcon(network);
  // Overlay the VPN badge if connecting to a VPN.
  if (network->type() != TYPE_VPN &&
      cros->virtual_network() && cros->virtual_network()->connecting()) {
    const gfx::ImageSkia* vpn_badge =
        rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_VPN_BADGE);
    const double animation = GetAnimation();
    animating = true;
    // Even though this is the only place we use vpn_connecting_badge_,
    // it is important that this is a member variable since we set a
    // pointer to it and access that pointer in icon_->GenerateImage().
    vpn_connecting_badge_ = gfx::ImageSkiaOperations::CreateBlendedImage(
        GetEmptyImage(vpn_badge->size()), *vpn_badge, animation);
    icon_->set_bottom_left_badge(&vpn_connecting_badge_);
  }

  // Set the text to display.
  if (network->type() == TYPE_ETHERNET) {
    if (mode_ == MENU_MODE) {
      text_ = l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
          l10n_util::GetStringUTF16(
              IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET));
    } else {
      text_ = l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    }
  } else {
    if (mode_ == MENU_MODE) {
      text_ = l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
          UTF8ToUTF16(network->name()));
    } else {
      text_ = UTF8ToUTF16(network->name());
    }
  }
  return animating;
}

void NetworkMenuIcon::SetDisconnectedIconAndText() {
  icon_->set_icon(GetDisconnectedImage(ARCS, resource_color_theme_));
  if (mode_ == MENU_MODE) {
    text_ = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_NOT_CONNECTED);
  } else {
    text_ = l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_NONE_SELECTED);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Static functions for generating network icon images:

// This defines how we assemble a network icon.
// Currently we iterate over all the available resolutions in |icon|. This will
// be wrong once we dynamically load image resolutions.
// TODO(pkotwicz): Figure out what to do when a new image resolution becomes
// available.
const gfx::ImageSkia NetworkMenuIcon::GenerateImageFromComponents(
    const gfx::ImageSkia& icon,
    const gfx::ImageSkia* top_left_badge,
    const gfx::ImageSkia* top_right_badge,
    const gfx::ImageSkia* bottom_left_badge,
    const gfx::ImageSkia* bottom_right_badge) {
  return gfx::ImageSkia(new NetworkIconImageSource(icon,
                                                   top_left_badge,
                                                   top_right_badge,
                                                   bottom_left_badge,
                                                   bottom_right_badge),
                   icon.size());
}

// We blend connecting icons with a black image to generate a faded icon.
const gfx::ImageSkia NetworkMenuIcon::GenerateConnectingImage(
    const gfx::ImageSkia& source) {
  return gfx::ImageSkiaOperations::CreateBlendedImage(
      GetEmptyImage(source.size()), source, kConnectingImageAlpha);
}

// Generates and caches an icon image for a network's current state.
const gfx::ImageSkia NetworkMenuIcon::GetImage(const Network* network,
                                               ResourceColorTheme color) {
  DCHECK(network);
  // Maintain a static (global) icon map. Note: Icons are never destroyed;
  // it is assumed that a finite and reasonable number of network icons will be
  // created during a session.

  typedef std::map<std::string, NetworkIcon*> NetworkIconMap;
  static NetworkIconMap* icon_map_dark = NULL;
  static NetworkIconMap* icon_map_light = NULL;
  if (icon_map_dark == NULL)
    icon_map_dark = new NetworkIconMap;
  if (icon_map_light == NULL)
    icon_map_light = new NetworkIconMap;

  NetworkIconMap* icon_map = color == COLOR_DARK ? icon_map_dark :
                                                   icon_map_light;
  // Find or add the icon.
  NetworkIcon* icon;
  NetworkIconMap::iterator iter = icon_map->find(network->service_path());
  if (iter == icon_map->end()) {
    icon = new NetworkIcon(network->service_path(), color);
    icon_map->insert(std::make_pair(network->service_path(), icon));
  } else {
    icon = iter->second;
  }
  // Update and return the icon's image.
  icon->Update();
  return icon->GetImage();
}

const gfx::ImageSkia NetworkMenuIcon::GetImage(ImageType type,
                                               int index,
                                               ResourceColorTheme color) {
  int width, height = 0;
  gfx::ImageSkia* images = NULL;
  if (type == NetworkMenuIcon::ARCS) {
    if (index >= kNumArcsImages)
      return gfx::ImageSkia();
    images = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        color == NetworkMenuIcon::COLOR_DARK ?
        IDR_AURA_UBER_TRAY_NETWORK_ARCS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_ARCS_LIGHT);
    width = images->width();
    height = images->height() / kNumArcsImages;
  } else {
    if (index >= kNumBarsImages)
      return gfx::ImageSkia();

    images = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        color == NetworkMenuIcon::COLOR_DARK ?
        IDR_AURA_UBER_TRAY_NETWORK_BARS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_BARS_LIGHT);
    width = images->width();
    height = images->height() / kNumBarsImages;
  }
  return gfx::ImageSkiaOperations::ExtractSubset(*images,
      gfx::Rect(0, index * height, width, height));
}

const gfx::ImageSkia NetworkMenuIcon::GetDisconnectedImage(
    ImageType type,
    ResourceColorTheme color) {
  return GetImage(type, 0, color);
}

const gfx::ImageSkia NetworkMenuIcon::GetConnectedImage(ImageType type,
      ResourceColorTheme color) {
  return GetImage(type, NumImages(type) - 1, color);
}

gfx::ImageSkia* NetworkMenuIcon::GetVirtualNetworkImage() {
  return ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_AURA_UBER_TRAY_NETWORK_VPN);
}

int NetworkMenuIcon::NumImages(ImageType type) {
  return (type == ARCS) ? kNumArcsImages : kNumBarsImages;
}

}  // chromeos
