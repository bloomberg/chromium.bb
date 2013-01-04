// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_policy_loader.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace extensions {

ExternalPolicyLoader::ExternalPolicyLoader(Profile* profile)
    : profile_(profile) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(prefs::kExtensionInstallForceList,
                             base::Bind(&ExternalPolicyLoader::StartLoading,
                                        base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kExtensionAllowedTypes,
                             base::Bind(&ExternalPolicyLoader::StartLoading,
                                        base::Unretained(this)));
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::Source<Profile>(profile_));
}

// static
void ExternalPolicyLoader::AddExtension(base::DictionaryValue* dict,
                                        const std::string& extension_id,
                                        const std::string& update_url) {
  dict->SetString(base::StringPrintf("%s.%s", extension_id.c_str(),
                                     ExternalProviderImpl::kExternalUpdateUrl),
                  update_url);
}

void ExternalPolicyLoader::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (profile_ == NULL) return;
  DCHECK(type == chrome::NOTIFICATION_PROFILE_DESTROYED) <<
      "Unexpected notification type.";
  if (content::Source<Profile>(source).ptr() == profile_) {
    notification_registrar_.RemoveAll();
    pref_change_registrar_.RemoveAll();
    profile_ = NULL;
  }
}

void ExternalPolicyLoader::StartLoading() {
  const DictionaryValue* forcelist =
      profile_->GetPrefs()->GetDictionary(prefs::kExtensionInstallForceList);
  prefs_.reset(forcelist ? forcelist->DeepCopy() : NULL);
  LoadFinished();
}

}  // namespace extensions
