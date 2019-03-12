// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"

namespace send_tab_to_self {

SendTabToSelfClientService::SendTabToSelfClientService(
    Profile* profile,
    SendTabToSelfModel* model) {
  model_ = model;
  model_->AddObserver(this);

  registry_ = ReceivingUiHandlerRegistry::GetInstance();
  registry_->InstantiatePlatformSpecificHandlers(profile);
}

SendTabToSelfClientService::~SendTabToSelfClientService() {
  model_->RemoveObserver(this);
  model_ = nullptr;
}

void SendTabToSelfClientService::SendTabToSelfModelLoaded() {
  // Do nothing for now
}

void SendTabToSelfClientService::EntriesAddedRemotely(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  // TODO(tgupta):Figure out whether it is better to pass in one entry at
  // a time or to pass all the entries at once.
  for (std::unique_ptr<ReceivingUiHandler>& handler :
       registry_->GetHandlers()) {
    handler->DisplayNewEntry(new_entries[0]);
  }
}

void SendTabToSelfClientService::EntriesRemovedRemotely(
    const std::vector<std::string>& guids) {
  for (std::unique_ptr<ReceivingUiHandler>& handler :
       registry_->GetHandlers()) {
    handler->DismissEntries(guids);
  }
}

}  // namespace send_tab_to_self
