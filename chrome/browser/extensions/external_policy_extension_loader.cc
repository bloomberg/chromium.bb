// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_policy_extension_loader.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"

namespace {

// Check an extension ID and an URL to be syntactically correct.
bool CheckExtension(const std::string& id, const std::string& update_url) {
  GURL url(update_url);
  if (!url.is_valid()) {
    LOG(WARNING) << "Policy specifies invalid update URL for external "
                 << "extension: " << update_url;
    return false;
  }
  if (!extensions::Extension::IdIsValid(id)) {
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
                              chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::Source<Profile>(profile_));
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
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (profile_ == NULL) return;
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED: {
      if (content::Source<PrefService>(source).ptr() == profile_->GetPrefs()) {
        std::string* pref_name = content::Details<std::string>(details).ptr();
        if (*pref_name == prefs::kExtensionInstallForceList) {
          StartLoading();
        } else {
          NOTREACHED() << "Unexpected preference name.";
        }
      }
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      if (content::Source<Profile>(source).ptr() == profile_) {
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
