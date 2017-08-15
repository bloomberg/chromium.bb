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
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {

// Width matches the Settings UI, height is sized to match the content.
const int kProxySettingsDialogWidth = 640;
const int kProxySettingsDialogHeight = 480;

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
                     GURL(chrome::kChromeUIProxySettingsURL)),
      guid_(network.guid()) {
  name_ = network.Matches(NetworkTypePattern::Ethernet())
              ? l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_ETHERNET)
              : base::UTF8ToUTF16(network.name());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ++instance_count_;
  SetDialogSize(kProxySettingsDialogWidth, kProxySettingsDialogHeight);
}

ProxySettingsDialog::~ProxySettingsDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  --instance_count_;
}

base::string16 ProxySettingsDialog::GetDialogTitle() const {
  return name_;
}

std::string ProxySettingsDialog::GetDialogArgs() const {
  return guid_;
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
