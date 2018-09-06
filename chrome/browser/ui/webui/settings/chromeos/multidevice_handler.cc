// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

namespace settings {

namespace {

const char kPageContentDataModeKey[] = "mode";
const char kPageContentDataHostDeviceNameKey[] = "hostDeviceName";
const char kPageContentDataBetterTogetherStateKey[] = "betterTogetherState";
const char kPageContentDataInstantTetheringStateKey[] = "instantTetheringState";
const char kPageContentDataMessagesStateKey[] = "messagesState";
const char kPageContentDataSmartLockStateKey[] = "smartLockState";

void OnRetrySetHostNowResult(bool success) {
  if (success)
    return;

  PA_LOG(WARNING) << "OnRetrySetHostNowResult(): Attempt to retry setting the "
                  << "host device failed.";
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
  web_ui()->RegisterMessageCallback(
      "setFeatureEnabledState",
      base::BindRepeating(&MultideviceHandler::HandleSetFeatureEnabledState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeHostDevice",
      base::BindRepeating(&MultideviceHandler::HandleRemoveHostDevice,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "retryPendingHostSetup",
      base::BindRepeating(&MultideviceHandler::HandleRetryPendingHostSetup,
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
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_status_with_device) {
  UpdatePageContent();
}

void MultideviceHandler::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  UpdatePageContent();
}

void MultideviceHandler::UpdatePageContent() {
  std::unique_ptr<base::DictionaryValue> page_content_dictionary =
      GeneratePageContentDataDictionary();
  DCHECK(page_content_dictionary);
  FireWebUIListener("settings.updateMultidevicePageContentData",
                    *page_content_dictionary);
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

  std::unique_ptr<base::DictionaryValue> page_content_dictionary =
      GeneratePageContentDataDictionary();
  DCHECK(page_content_dictionary);

  ResolveJavascriptCallback(base::Value(callback_id), *page_content_dictionary);
}

void MultideviceHandler::HandleSetFeatureEnabledState(
    const base::ListValue* args) {
  std::string callback_id;
  bool result = args->GetString(0, &callback_id);
  DCHECK(result);

  int feature_as_int;
  result = args->GetInteger(1, &feature_as_int);
  DCHECK(result);

  auto feature = static_cast<multidevice_setup::mojom::Feature>(feature_as_int);
  DCHECK(multidevice_setup::mojom::IsKnownEnumValue(feature));

  bool enabled;
  result = args->GetBoolean(2, &enabled);
  DCHECK(result);

  base::Optional<std::string> auth_token;
  std::string possible_token_value;
  if (args->GetString(3, &possible_token_value))
    auth_token = possible_token_value;

  multidevice_setup_client_->SetFeatureEnabledState(
      feature, enabled, auth_token,
      base::BindOnce(&MultideviceHandler::OnSetFeatureStateEnabledResult,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void MultideviceHandler::HandleRemoveHostDevice(const base::ListValue* args) {
  DCHECK(args->empty());
  multidevice_setup_client_->RemoveHostDevice();
}

void MultideviceHandler::HandleRetryPendingHostSetup(
    const base::ListValue* args) {
  DCHECK(args->empty());
  multidevice_setup_client_->RetrySetHostNow(
      base::BindOnce(&OnRetrySetHostNowResult));
}

void MultideviceHandler::OnSetFeatureStateEnabledResult(
    const std::string& js_callback_id,
    bool success) {
  ResolveJavascriptCallback(base::Value(js_callback_id), base::Value(success));
}

std::unique_ptr<base::DictionaryValue>
MultideviceHandler::GeneratePageContentDataDictionary() {
  auto page_content_dictionary = std::make_unique<base::DictionaryValue>();

  multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
      host_status_with_device = multidevice_setup_client_->GetHostStatus();
  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap feature_states =
      multidevice_setup_client_->GetFeatureStates();

  page_content_dictionary->SetInteger(
      kPageContentDataModeKey,
      static_cast<int32_t>(host_status_with_device.first));
  page_content_dictionary->SetInteger(
      kPageContentDataBetterTogetherStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kBetterTogetherSuite]));
  page_content_dictionary->SetInteger(
      kPageContentDataInstantTetheringStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kInstantTethering]));
  page_content_dictionary->SetInteger(
      kPageContentDataMessagesStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kMessages]));
  page_content_dictionary->SetInteger(
      kPageContentDataSmartLockStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kSmartLock]));

  if (host_status_with_device.second) {
    page_content_dictionary->SetString(kPageContentDataHostDeviceNameKey,
                                       host_status_with_device.second->name());
  }

  return page_content_dictionary;
}

}  // namespace settings

}  // namespace chromeos
