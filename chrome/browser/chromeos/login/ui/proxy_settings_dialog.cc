// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/proxy_settings_dialog.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_type_pattern.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/base/escape.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace {

// Hints for size of proxy settings dialog.
const int kProxySettingsDialogReasonableWidth = 626;
const int kProxySettingsDialogReasonableHeight = 525;
const float kProxySettingsDialogReasonableWidthRatio = 0.4f;
const float kProxySettingsDialogReasonableHeightRatio = 0.4f;

const char* kProxySettingsURLParam = "?network=%s";

int CalculateSize(int screen_size, int min_comfortable, float desired_ratio) {
  int desired_size = static_cast<int>(desired_ratio * screen_size);
  desired_size = std::max(min_comfortable, desired_size);
  return std::min(screen_size, desired_size);
}

GURL GetURLForProxySettings(const std::string& service_path) {
  std::string url(chrome::kChromeUIProxySettingsURL);
  url += base::StringPrintf(
      kProxySettingsURLParam,
      net::EscapeUrlEncodedData(service_path, true).c_str());
  return GURL(url);
}

}  // namespace

namespace chromeos {

// static
int ProxySettingsDialog::instance_count_ = 0;

ProxySettingsDialog::ProxySettingsDialog(
    content::BrowserContext* browser_context,
    const NetworkState& network,
    LoginWebDialog::Delegate* delegate,
    gfx::NativeWindow window)
    : LoginWebDialog(browser_context,
                     delegate,
                     window,
                     base::string16(),
                     GetURLForProxySettings(network.path()),
                     LoginWebDialog::STYLE_BUBBLE) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ++instance_count_;

  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
  SetDialogSize(CalculateSize(screen_bounds.width(),
                              kProxySettingsDialogReasonableWidth,
                              kProxySettingsDialogReasonableWidthRatio),
                CalculateSize(screen_bounds.height(),
                              kProxySettingsDialogReasonableHeight,
                              kProxySettingsDialogReasonableHeightRatio));

  std::string network_name = network.name();
  if (network_name.empty() && network.Matches(NetworkTypePattern::Ethernet())) {
    network_name =
        l10n_util::GetStringUTF8(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  }

  SetDialogTitle(l10n_util::GetStringFUTF16(IDS_PROXY_PAGE_TITLE_FORMAT,
                                            base::ASCIIToUTF16(network_name)));
}

ProxySettingsDialog::~ProxySettingsDialog() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  --instance_count_;
}

void ProxySettingsDialog::OnDialogClosed(const std::string& json_retval) {
  LoginWebDialog::OnDialogClosed(json_retval);
  content::NotificationService::current()->Notify(
    chrome::NOTIFICATION_LOGIN_PROXY_CHANGED,
    content::NotificationService::AllSources(),
    content::NotificationService::NoDetails());
}

bool ProxySettingsDialog::IsShown() {
  return instance_count_ > 0;
}

}  // namespace chromeos
