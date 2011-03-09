// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_policy_extension_loader.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "googleurl/src/gurl.h"

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

}  // namespace

ExternalPolicyExtensionLoader::ExternalPolicyExtensionLoader(
    Profile* profile)
    : profile_(profile) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(prefs::kExtensionInstallForceList, this);
  notification_registrar_.Add(this,
                              NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
}

void ExternalPolicyExtensionLoader::StartLoading() {
  const ListValue* forcelist =
      profile_->GetPrefs()->GetList(prefs::kExtensionInstallForceList);
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
  prefs_.reset(result);
  LoadFinished();
}

void ExternalPolicyExtensionLoader::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (profile_ == NULL) return;
  switch (type.value) {
    case NotificationType::PREF_CHANGED: {
      if (Source<PrefService>(source).ptr() == profile_->GetPrefs()) {
        std::string* pref_name = Details<std::string>(details).ptr();
        if (*pref_name == prefs::kExtensionInstallForceList) {
          StartLoading();
        } else {
          NOTREACHED() << "Unexpected preference name.";
        }
      }
      break;
    }
    case NotificationType::PROFILE_DESTROYED: {
      if (Source<Profile>(source).ptr() == profile_) {
        notification_registrar_.RemoveAll();
        pref_change_registrar_.RemoveAll();
        profile_ = NULL;
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type.";
  }
}
