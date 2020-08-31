// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_web_dialog.h"

#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

const float kNetworkPortalWebDialogWidthFraction = .8;
const float kNetworkPortalWebDialogHeightFraction = .8;

gfx::Size GetPortalDialogSize() {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size display_size = display.size();

  if (display.rotation() == display::Display::ROTATE_90 ||
      display.rotation() == display::Display::ROTATE_270) {
    display_size = gfx::Size(display_size.height(), display_size.width());
  }

  display_size =
      gfx::Size(display_size.width() * kNetworkPortalWebDialogWidthFraction,
                display_size.height() * kNetworkPortalWebDialogHeightFraction);

  return display_size;
}

}  // namespace

namespace chromeos {

NetworkPortalWebDialog::NetworkPortalWebDialog(
    base::WeakPtr<NetworkPortalNotificationController> controller)
    : controller_(controller), widget_(nullptr) {
}

NetworkPortalWebDialog::~NetworkPortalWebDialog() {
  if (controller_)
    controller_->OnDialogDestroyed(this);
}

void NetworkPortalWebDialog::Close() {
  if (widget_)
    widget_->Close();
}

void NetworkPortalWebDialog::SetWidget(views::Widget* widget) {
  widget_ = widget;
}

ui::ModalType NetworkPortalWebDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 NetworkPortalWebDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_CAPTIVE_PORTAL_AUTHORIZATION_DIALOG_NAME);
}

GURL NetworkPortalWebDialog::GetDialogContentURL() const {
  return GURL(captive_portal::CaptivePortalDetector::kDefaultURL);
}

void NetworkPortalWebDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {
}

void NetworkPortalWebDialog::GetDialogSize(gfx::Size* size) const {
  *size = GetPortalDialogSize();
}

std::string NetworkPortalWebDialog::GetDialogArgs() const {
  return std::string();
}

bool NetworkPortalWebDialog::CanResizeDialog() const {
  return false;
}

void NetworkPortalWebDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void NetworkPortalWebDialog::OnCloseContents(content::WebContents* /* source */,
                                             bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool NetworkPortalWebDialog::ShouldShowDialogTitle() const {
  return true;
}

}  // namespace chromeos
