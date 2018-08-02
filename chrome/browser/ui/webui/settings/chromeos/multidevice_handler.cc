// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

namespace settings {

namespace {

const char kPageContentDataModeKey[] = "mode";
const char kPageContentDataHostDeviceKey[] = "hostDevice";

const char kRemoteDeviceNameKey[] = "name";
const char kRemoteDeviceSoftwareFeaturesKey[] = "softwareFeatures";

std::unique_ptr<base::DictionaryValue> GeneratePageContentDataDictionary(
    multidevice_setup::mojom::HostStatus host_status,
    const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
  auto page_content_dictionary = std::make_unique<base::DictionaryValue>();
  page_content_dictionary->SetInteger(kPageContentDataModeKey,
                                      static_cast<int32_t>(host_status));

  if (host_device) {
    auto device_dictionary = std::make_unique<base::DictionaryValue>();
    device_dictionary->SetString(kRemoteDeviceNameKey, host_device->name());

    // TODO(khorimoto): Send actual feature dictionary. Currently, an empty
    // dictionary is passed.
    device_dictionary->SetDictionary(kRemoteDeviceSoftwareFeaturesKey,
                                     std::make_unique<base::DictionaryValue>());

    page_content_dictionary->SetDictionary(kPageContentDataHostDeviceKey,
                                           std::move(device_dictionary));
  }

  return page_content_dictionary;
}

}  // namespace

MultideviceHandler::MultideviceHandler(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : multidevice_setup_client_(multidevice_setup_client),
      multidevice_setup_observer_(this),
      callback_weak_ptr_factory_(this) {}

MultideviceHandler::~MultideviceHandler() {}

void MultideviceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showMultiDeviceSetupDialog",
      base::BindRepeating(&MultideviceHandler::HandleShowMultiDeviceSetupDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPageContentData",
      base::BindRepeating(&MultideviceHandler::HandleGetPageContent,
                          base::Unretained(this)));
}

void MultideviceHandler::OnJavascriptAllowed() {
  multidevice_setup_observer_.Add(multidevice_setup_client_);
}

void MultideviceHandler::OnJavascriptDisallowed() {
  multidevice_setup_observer_.Remove(multidevice_setup_client_);

  // Ensure that pending callbacks do not complete and cause JS to be evaluated.
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
}

void MultideviceHandler::OnHostStatusChanged(
    multidevice_setup::mojom::HostStatus host_status,
    const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
  FireWebUIListener(
      "settings.updateMultidevicePageContentData",
      *GeneratePageContentDataDictionary(host_status, host_device));
}

void MultideviceHandler::HandleShowMultiDeviceSetupDialog(
    const base::ListValue* args) {
  DCHECK(args->empty());
  multidevice_setup::MultiDeviceSetupDialog::Show();
}

void MultideviceHandler::HandleGetPageContent(const base::ListValue* args) {
  // This callback is expected to be the first one executed when the page is
  // loaded, so it should be the one to allow JS calls.
  AllowJavascript();

  std::string callback_id;
  bool result = args->GetString(0, &callback_id);
  DCHECK(result);

  multidevice_setup_client_->GetHostStatus(
      base::BindOnce(&MultideviceHandler::OnHostStatusFetched,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void MultideviceHandler::OnHostStatusFetched(
    const std::string& js_callback_id,
    multidevice_setup::mojom::HostStatus host_status,
    const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
  ResolveJavascriptCallback(
      base::Value(js_callback_id),
      *GeneratePageContentDataDictionary(host_status, host_device));
}

}  // namespace settings

}  // namespace chromeos
