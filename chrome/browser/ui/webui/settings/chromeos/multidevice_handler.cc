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
    multidevice_setup::mojom::HostStatus host_status,
    const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
  last_host_status_update_ = std::make_pair(host_status, host_device);
  AttemptUpdatePageContent();
}

void MultideviceHandler::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  last_feature_states_update_ = feature_states_map;
  AttemptUpdatePageContent();
}

void MultideviceHandler::AttemptGetPageContentResponse(
    const std::string& js_callback_id) {
  std::unique_ptr<base::DictionaryValue> page_content_dictionary =
      GeneratePageContentDataDictionary();
  if (!page_content_dictionary)
    return;

  ResolveJavascriptCallback(base::Value(js_callback_id),
                            *page_content_dictionary);
}

void MultideviceHandler::AttemptUpdatePageContent() {
  std::unique_ptr<base::DictionaryValue> page_content_dictionary =
      GeneratePageContentDataDictionary();
  if (!page_content_dictionary)
    return;

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

  if (last_host_status_update_ && last_feature_states_update_) {
    AttemptGetPageContentResponse(callback_id);
    return;
  }

  multidevice_setup_client_->GetHostStatus(
      base::BindOnce(&MultideviceHandler::OnHostStatusFetched,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
  multidevice_setup_client_->GetFeatureStates(
      base::BindOnce(&MultideviceHandler::OnFeatureStatesFetched,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void MultideviceHandler::HandleRetryPendingHostSetup(
    const base::ListValue* args) {
  DCHECK(args->empty());
  multidevice_setup_client_->RetrySetHostNow(
      base::BindOnce(&OnRetrySetHostNowResult));
}

void MultideviceHandler::OnHostStatusFetched(
    const std::string& js_callback_id,
    multidevice_setup::mojom::HostStatus host_status,
    const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
  last_host_status_update_ = std::make_pair(host_status, host_device);
  AttemptGetPageContentResponse(js_callback_id);
}

void MultideviceHandler::OnFeatureStatesFetched(
    const std::string& js_callback_id,
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  last_feature_states_update_ = feature_states_map;
  AttemptGetPageContentResponse(js_callback_id);
}

std::unique_ptr<base::DictionaryValue>
MultideviceHandler::GeneratePageContentDataDictionary() {
  // Cannot generate page contents without all required data.
  if (!last_host_status_update_ || !last_feature_states_update_)
    return nullptr;

  auto page_content_dictionary = std::make_unique<base::DictionaryValue>();

  page_content_dictionary->SetInteger(
      kPageContentDataModeKey,
      static_cast<int32_t>(last_host_status_update_->first));
  page_content_dictionary->SetInteger(
      kPageContentDataBetterTogetherStateKey,
      static_cast<int32_t>(
          (*last_feature_states_update_)
              [multidevice_setup::mojom::Feature::kBetterTogetherSuite]));
  page_content_dictionary->SetInteger(
      kPageContentDataInstantTetheringStateKey,
      static_cast<int32_t>(
          (*last_feature_states_update_)
              [multidevice_setup::mojom::Feature::kInstantTethering]));
  page_content_dictionary->SetInteger(
      kPageContentDataMessagesStateKey,
      static_cast<int32_t>((*last_feature_states_update_)
                               [multidevice_setup::mojom::Feature::kMessages]));
  page_content_dictionary->SetInteger(
      kPageContentDataSmartLockStateKey,
      static_cast<int32_t>(
          (*last_feature_states_update_)
              [multidevice_setup::mojom::Feature::kSmartLock]));

  if (last_host_status_update_->second) {
    page_content_dictionary->SetString(
        kPageContentDataHostDeviceNameKey,
        last_host_status_update_->second->name());
  }

  return page_content_dictionary;
}

}  // namespace settings

}  // namespace chromeos
