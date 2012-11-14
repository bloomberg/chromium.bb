// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_warning_service.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

namespace extensions {

ExtensionWarningService::ExtensionWarningService(Profile* profile)
    : profile_(profile) {
  DCHECK(CalledOnValidThread());
  if (profile_) {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
        content::Source<Profile>(profile_->GetOriginalProfile()));
  }
}

ExtensionWarningService::~ExtensionWarningService() {}

void ExtensionWarningService::ClearWarnings(
    const std::set<ExtensionWarning::WarningType>& types) {
  DCHECK(CalledOnValidThread());
  bool deleted_anything = false;
  for (ExtensionWarningSet::iterator i = warnings_.begin();
       i != warnings_.end();) {
    if (types.find(i->warning_type()) != types.end()) {
      deleted_anything = true;
      warnings_.erase(i++);
    } else {
      ++i;
    }
  }

  if (deleted_anything)
    NotifyWarningsChanged();
}

std::set<ExtensionWarning::WarningType>
ExtensionWarningService::GetWarningTypesAffectingExtension(
    const std::string& extension_id) const {
  DCHECK(CalledOnValidThread());
  std::set<ExtensionWarning::WarningType> result;
  for (ExtensionWarningSet::const_iterator i = warnings_.begin();
       i != warnings_.end(); ++i) {
    if (i->extension_id() == extension_id)
      result.insert(i->warning_type());
  }
  return result;
}

std::vector<std::string>
ExtensionWarningService::GetWarningMessagesForExtension(
    const std::string& extension_id) const {
  DCHECK(CalledOnValidThread());
  std::vector<std::string> result;

  const ExtensionService* extension_service =
      ExtensionSystem::Get(profile_)->extension_service();

  for (ExtensionWarningSet::const_iterator i = warnings_.begin();
       i != warnings_.end(); ++i) {
    if (i->extension_id() == extension_id)
      result.push_back(i->GetLocalizedMessage(extension_service->extensions()));
  }
  return result;
}

void ExtensionWarningService::AddWarnings(
    const ExtensionWarningSet& warnings) {
  DCHECK(CalledOnValidThread());
  size_t old_size = warnings_.size();

  warnings_.insert(warnings.begin(), warnings.end());

  if (old_size != warnings_.size())
    NotifyWarningsChanged();
}

// static
void ExtensionWarningService::NotifyWarningsOnUI(
    void* profile_id,
    const ExtensionWarningSet& warnings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!profile ||
      !g_browser_process->profile_manager() ||
      !g_browser_process->profile_manager()->IsValidProfile(profile)) {
    return;
  }

  extensions::ExtensionWarningService* warning_service =
      extensions::ExtensionSystem::Get(profile)->warning_service();

  warning_service->AddWarnings(warnings);
}

void ExtensionWarningService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ExtensionWarningService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ExtensionWarningService::NotifyWarningsChanged() {
  FOR_EACH_OBSERVER(Observer, observer_list_, ExtensionWarningsChanged());
}

void ExtensionWarningService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<extensions::UnloadedExtensionInfo>(details)->
          extension;
      // Unloading one extension might have solved the problems of others.
      // Therefore, we clear warnings of this type for all extensions.
      std::set<ExtensionWarning::WarningType> warning_types =
          GetWarningTypesAffectingExtension(extension->id());
      ClearWarnings(warning_types);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace extensions
