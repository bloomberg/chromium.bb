// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/serial/serial_chooser_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

blink::mojom::SerialPortInfoPtr ToBlinkType(
    const device::mojom::SerialPortInfo& port) {
  auto info = blink::mojom::SerialPortInfo::New();
  info->token = port.token;
  info->has_vendor_id = port.has_vendor_id;
  if (port.has_vendor_id)
    info->vendor_id = port.vendor_id;
  info->has_product_id = port.has_product_id;
  if (port.has_product_id)
    info->product_id = port.product_id;
  return info;
}

}  // namespace

SerialChooserController::SerialChooserController(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    content::SerialChooser::Callback callback)
    : ChooserController(render_frame_host,
                        IDS_SERIAL_PORT_CHOOSER_PROMPT_ORIGIN,
                        IDS_SERIAL_PORT_CHOOSER_PROMPT_EXTENSION_NAME),
      filters_(std::move(filters)),
      callback_(std::move(callback)) {
  DCHECK(content::ServiceManagerConnection::GetForProcess());
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(device::mojom::kServiceName,
                      mojo::MakeRequest(&port_manager_));
  port_manager_.set_connection_error_handler(base::BindOnce(
      &SerialChooserController::OnGetDevices, base::Unretained(this),
      std::vector<device::mojom::SerialPortInfoPtr>()));
  port_manager_->GetDevices(base::BindOnce(
      &SerialChooserController::OnGetDevices, base::Unretained(this)));
}

SerialChooserController::~SerialChooserController() {
  if (callback_)
    std::move(callback_).Run(nullptr);
}

base::string16 SerialChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

base::string16 SerialChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_SERIAL_PORT_CHOOSER_CONNECT_BUTTON_TEXT);
}

size_t SerialChooserController::NumOptions() const {
  return ports_.size();
}

base::string16 SerialChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, ports_.size());
  const device::mojom::SerialPortInfo& port = *ports_[index];

  // Get the last component of the device path i.e. COM1 or ttyS0 to show the
  // user something similar to other applications that ask them to choose a
  // serial port and to differentiate between ports with similar display names.
  base::string16 display_path = port.path.BaseName().LossyDisplayName();

  if (port.display_name) {
    return l10n_util::GetStringFUTF16(IDS_SERIAL_PORT_CHOOSER_NAME_WITH_PATH,
                                      base::UTF8ToUTF16(*port.display_name),
                                      display_path);
  }

  return l10n_util::GetStringFUTF16(IDS_SERIAL_PORT_CHOOSER_PATH_ONLY,
                                    display_path);
}

bool SerialChooserController::IsPaired(size_t index) const {
  return false;
}

void SerialChooserController::Select(const std::vector<size_t>& indices) {
  DCHECK_EQ(1u, indices.size());
  size_t index = indices[0];
  DCHECK_LT(index, ports_.size());

  const device::mojom::SerialPortInfo& port = *ports_[index];
  std::move(callback_).Run(ToBlinkType(port));
}

void SerialChooserController::Cancel() {}

void SerialChooserController::Close() {}

void SerialChooserController::OpenHelpCenterUrl() const {
  NOTIMPLEMENTED();
}

void SerialChooserController::OnGetDevices(
    std::vector<device::mojom::SerialPortInfoPtr> ports) {
  for (auto& port : ports) {
    if (FilterMatchesAny(*port))
      ports_.push_back(std::move(port));
  }

  if (view())
    view()->OnOptionsInitialized();
}

bool SerialChooserController::FilterMatchesAny(
    const device::mojom::SerialPortInfo& port) const {
  if (filters_.empty())
    return true;

  for (const auto& filter : filters_) {
    if (filter->has_vendor_id &&
        (!port.has_vendor_id || filter->vendor_id != port.vendor_id)) {
      continue;
    }
    if (filter->has_product_id &&
        (!port.has_product_id || filter->product_id != port.product_id)) {
      continue;
    }
    return true;
  }

  return false;
}
