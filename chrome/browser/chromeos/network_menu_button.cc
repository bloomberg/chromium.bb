// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_menu_button.h"

#include <limits>

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton

// static
const int NetworkMenuButton::kNumWifiImages = 3;
const int NetworkMenuButton::kMinOpacity = 50;
const int NetworkMenuButton::kMaxOpacity = 256;
const int NetworkMenuButton::kThrobDuration = 1000;

NetworkMenuButton::NetworkMenuButton(gfx::NativeWindow browser_window)
    : StatusAreaButton(this),
      ALLOW_THIS_IN_INITIALIZER_LIST(network_menu_(this)),
      browser_window_(browser_window),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_connecting_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_downloading_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_uploading_(this)) {
  animation_connecting_.SetThrobDuration(kThrobDuration);
  animation_connecting_.SetTweenType(SlideAnimation::NONE);
  animation_downloading_.SetThrobDuration(kThrobDuration);
  animation_downloading_.SetTweenType(SlideAnimation::NONE);
  animation_uploading_.SetThrobDuration(kThrobDuration);
  animation_uploading_.SetTweenType(SlideAnimation::NONE);
  NetworkChanged(NetworkLibrary::Get());
  NetworkLibrary::Get()->AddObserver(this);
}

NetworkMenuButton::~NetworkMenuButton() {
  NetworkLibrary::Get()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::Menu2Model implementation:

int NetworkMenuButton::GetItemCount() const {
  // The menu contains the available wifi networks. If there are none, then it
  // only has one item with a message that no networks are available.
  return wifi_networks_.empty() ? 1 : static_cast<int>(wifi_networks_.size());
}

views::Menu2Model::ItemType NetworkMenuButton::GetTypeAt(int index) const {
  return wifi_networks_.empty() ? views::Menu2Model::TYPE_COMMAND :
      views::Menu2Model::TYPE_CHECK;
}

string16 NetworkMenuButton::GetLabelAt(int index) const {
  return wifi_networks_.empty() ?
      l10n_util::GetStringUTF16(IDS_STATUSBAR_NO_NETWORKS_MESSAGE) :
      ASCIIToUTF16(wifi_networks_[index].ssid);
}

bool NetworkMenuButton::IsItemCheckedAt(int index) const {
  // WifiNetwork that we are connected to (or connecting to) is checked.
  return wifi_networks_.empty() ? false :
      wifi_networks_[index].ssid == NetworkLibrary::Get()->wifi_ssid();
}

bool NetworkMenuButton::IsEnabledAt(int index) const {
  return !wifi_networks_.empty();
}

void NetworkMenuButton::ActivatedAt(int index) {
  // When we are refreshing the menu, ignore menu item activation.
  if (refreshing_menu_)
    return;

  NetworkLibrary* cros = NetworkLibrary::Get();

  // If clicked on a network that we are already connected to or we are
  // currently trying to connect to, then do nothing.
  if (wifi_networks_[index].ssid == cros->wifi_ssid())
    return;

  activated_wifi_network_ = wifi_networks_[index];

  // If wifi network is not encrypted, then directly connect.
  // Otherwise, we open password dialog window.
  if (!wifi_networks_[index].encrypted) {
    cros->ConnectToWifiNetwork(wifi_networks_[index], string16());
  } else {
    PasswordDialogView* dialog = new PasswordDialogView(this,
        wifi_networks_[index].ssid);
    views::Window* window = views::Window::CreateChromeWindow(
        browser_window_, gfx::Rect(), dialog);
    // Draw the password dialog right below this button and right aligned.
    gfx::Size size = dialog->GetPreferredSize();
    gfx::Rect rect = bounds();
    gfx::Point point = gfx::Point(rect.width() - size.width(), rect.height());
    ConvertPointToScreen(this, &point);
    window->SetBounds(gfx::Rect(point, size), browser_window_);
    window->Show();
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, PasswordDialogDelegate implementation:

bool NetworkMenuButton::OnPasswordDialogAccept(const std::string& ssid,
                                               const string16& password) {
  NetworkLibrary::Get()->ConnectToWifiNetwork(activated_wifi_network_,
                                              password);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, AnimationDelegate implementation:

void NetworkMenuButton::AnimationProgressed(const Animation* animation) {
  if (animation == &animation_connecting_ ||
      animation == &animation_downloading_ ||
      animation == &animation_uploading_)
    SchedulePaint();
  else
    MenuButton::AnimationProgressed(animation);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, StatusAreaButton implementation:

// Override the DrawIcon method to draw the wifi icon.
// The wifi icon is composed of 1 or more alpha-blended icons to show the
// network strength. We also draw an animation for when there's upload/download
// traffic.
void NetworkMenuButton::DrawIcon(gfx::Canvas* canvas) {
  // First draw the base icon.
  canvas->DrawBitmapInt(icon(), 0, 0);

  // If wifi, we draw the wifi signal bars.
  NetworkLibrary* cros = NetworkLibrary::Get();
  if (cros->wifi_connecting() ||
          (!cros->ethernet_connected() && !cros->wifi_ssid().empty())) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    // We want a value between 0-1.
    // 0 reperesents no signal and 1 represents full signal strength.
    double value = cros->wifi_connecting() ?
        animation_connecting_.GetCurrentValue() : cros->wifi_strength() / 100.0;
    if (value < 0)
      value = 0;
    else if (value > 1)
      value = 1;

    // If we are animating network traffic and not connecting, then we need to
    // figure out if we are to also draw the extra image.
    int downloading_index = -1;
    int uploading_index = -1;
    if (!animation_connecting_.IsAnimating()) {
      // For network animation, we only show animation in one direction.
      // So when we are hiding, we just use 1 minus the value.
      // We have kNumWifiImages + 1 number of states. For the first state, where
      // we are not adding any images, we set the index to -1.
      if (animation_downloading_.IsAnimating()) {
        double value_downloading = animation_downloading_.IsShowing() ?
            animation_downloading_.GetCurrentValue() :
            1.0 - animation_downloading_.GetCurrentValue();
        downloading_index = static_cast<int>(value_downloading *
              nextafter(static_cast<float>(kNumWifiImages + 1), 0)) - 1;
      }
      if (animation_uploading_.IsAnimating()) {
        double value_uploading = animation_uploading_.IsShowing() ?
            animation_uploading_.GetCurrentValue() :
            1.0 - animation_uploading_.GetCurrentValue();
        uploading_index = static_cast<int>(value_uploading *
              nextafter(static_cast<float>(kNumWifiImages + 1), 0)) - 1;
      }
    }

    // We need to determine opacity for each of the kNumWifiImages images.
    // We split the range (0-1) into equal ranges per kNumWifiImages images.
    // For example if kNumWifiImages is 3, then [0-0.33) is the first image and
    // [0.33-0.66) is the second image and [0.66-1] is the last image.
    // For each of the image:
    //   If value < the range of this image, draw at kMinOpacity opacity.
    //   If value > the range of this image, draw at kMaxOpacity-1 opacity.
    //   If value within the range of this image, draw at an opacity value
    //     between kMinOpacity and kMaxOpacity-1 relative to where in the range
    //     value is at.
    double value_per_image = 1.0 / kNumWifiImages;
    SkPaint paint;
    for (int i = 0; i < kNumWifiImages; i++) {
      if (value > value_per_image) {
        paint.setAlpha(kMaxOpacity - 1);
        value -= value_per_image;
      } else {
        // Map value between 0 and value_per_image to [kMinOpacity,kMaxOpacity).
        paint.setAlpha(kMinOpacity + static_cast<int>(value / value_per_image *
            nextafter(static_cast<float>(kMaxOpacity - kMinOpacity), 0)));
        // For following iterations, we want to draw at kMinOpacity.
        // So we set value to 0 here.
        value = 0;
      }
      canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_UP1 + i),
                            0, 0, paint);
      canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_DOWN1 + i),
                            0, 0, paint);

      // Draw network traffic downloading/uploading image if necessary.
      if (i == downloading_index)
        canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_DOWN1P + i),
                              0, 0, paint);
      if (i == uploading_index)
        canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_UP1P + i),
                              0, 0, paint);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::ViewMenuDelegate implementation:

void NetworkMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  wifi_networks_ = NetworkLibrary::Get()->wifi_networks();
  refreshing_menu_ = true;
  network_menu_.Rebuild();
  network_menu_.UpdateStates();
  refreshing_menu_ = false;
  network_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::Observer implementation:

void NetworkMenuButton::NetworkChanged(NetworkLibrary* cros) {
  int id = IDR_STATUSBAR_WARNING;
  if (cros->loaded()) {
    id = IDR_STATUSBAR_NETWORK_DISCONNECTED;
    if (cros->wifi_connecting()) {
      // Start the connecting animation if not running.
      if (!animation_connecting_.IsAnimating()) {
        animation_connecting_.Reset();
        animation_connecting_.StartThrobbing(std::numeric_limits<int>::max());
      }
      // Stop network traffic animation when we are connecting.
      animation_downloading_.Stop();
      animation_uploading_.Stop();

      id = IDR_STATUSBAR_WIFI_DOT;
    } else {
      // Stop connecting animation since we are not connecting.
      animation_connecting_.Stop();

      // Always show the higher priority connection first. Ethernet then wifi.
      if (cros->ethernet_connected()) {
        id = IDR_STATUSBAR_WIRED;
      } else if (!cros->wifi_ssid().empty()) {
        id = IDR_STATUSBAR_WIFI_DOT;
      }
    }
  }

  SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(id));
  SchedulePaint();
}

void NetworkMenuButton::NetworkTraffic(NetworkLibrary* cros, int traffic_type) {
  if (!cros->ethernet_connected() && !cros->wifi_ssid().empty() &&
      !cros->wifi_connecting()) {
    // For downloading/uploading animation, we want to force at least one cycle
    // so that it looks smooth. And if we keep downloading/uploading, we will
    // keep calling StartThrobbing which will update the cycle count back to 2.
    if (traffic_type & TRAFFIC_DOWNLOAD)
      animation_downloading_.StartThrobbing(2);
    if (traffic_type & TRAFFIC_UPLOAD)
      animation_uploading_.StartThrobbing(2);
  }
}

}  // namespace chromeos
