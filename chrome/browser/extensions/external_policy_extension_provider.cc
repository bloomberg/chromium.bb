// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_policy_extension_provider.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/stateful_external_extension_provider.h"
#include "chrome/browser/prefs/pref_service.h"

namespace {

// Check an extension ID and an URL to be syntactically correct.
bool CheckExtension(std::string id, std::string update_url) {
  GURL url(update_url);
  if (!url.is_valid()) {
    LOG(WARNING) << "Policy specifies invalid update URL for external "
                 << "extension: " << update_url;
    return false;
  }
  if (!Extension::IdIsValid(id)) {
    LOG(WARNING) << "Policy specifies invalid ID for external "
                 << "extension: " << id;
    return false;
  }
  return true;
}

}

ExternalPolicyExtensionProvider::ExternalPolicyExtensionProvider(
    const ListValue* forcelist)
        : StatefulExternalExtensionProvider(
            Extension::INVALID,
            Extension::EXTERNAL_POLICY_DOWNLOAD) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProcessPreferences(forcelist);
}

ExternalPolicyExtensionProvider::~ExternalPolicyExtensionProvider() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ExternalPolicyExtensionProvider::SetPreferences(
    const ListValue* forcelist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  ProcessPreferences(forcelist);
}

void ExternalPolicyExtensionProvider::ProcessPreferences(
    const ListValue* forcelist) {
  DictionaryValue* result = new DictionaryValue();
  if (forcelist != NULL) {
    std::string extension_desc;
    for (ListValue::const_iterator it = forcelist->begin();
         it != forcelist->end(); ++it) {
      if (!(*it)->GetAsString(&extension_desc)) {
        LOG(WARNING) << "Failed to read forcelist string.";
      } else {
        // Each string item of the list has the following form:
        // extension_id_code;extension_update_url
        // The update URL might also contain semicolons.
        size_t pos = extension_desc.find(';');
        std::string id = extension_desc.substr(0, pos);
        std::string update_url = extension_desc.substr(pos+1);
        if (CheckExtension(id, update_url)) {
          result->SetString(id + ".external_update_url", update_url);
        }
      }
    }
  }
  set_prefs(result);
}
