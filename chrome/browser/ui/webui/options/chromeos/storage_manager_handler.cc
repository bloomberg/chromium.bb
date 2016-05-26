// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/storage_manager_handler.h"

#include "chrome/grit/generated_resources.h"

namespace chromeos {
namespace options {

StorageManagerHandler::StorageManagerHandler() {
}

StorageManagerHandler::~StorageManagerHandler() {
}

void StorageManagerHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "storageManagerPage",
                IDS_OPTIONS_SETTINGS_STORAGE_MANAGER_TAB_TITLE);
}

void StorageManagerHandler::InitializePage() {
  DCHECK(web_ui());
}

void StorageManagerHandler::RegisterMessages() {
  DCHECK(web_ui());
}

}  // namespace options
}  // namespace chromeos
