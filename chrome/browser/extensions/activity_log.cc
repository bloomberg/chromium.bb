// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

namespace extensions {

ActivityLog::ActivityLog() {
  log_activity_to_stdout_ = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableExtensionActivityLogging);
}

ActivityLog::~ActivityLog() {
}

// static
ActivityLog* ActivityLog::GetInstance() {
  return Singleton<ActivityLog>::get();
}

void ActivityLog::AddObserver(const Extension* extension,
                              ActivityLog::Observer* observer) {
  base::AutoLock scoped_lock(lock_);

  if (observers_.count(extension) == 0) {
    observers_[extension] = new ObserverListThreadSafe<Observer>;
  }

  observers_[extension]->AddObserver(observer);
}

void ActivityLog::RemoveObserver(const Extension* extension,
                                 ActivityLog::Observer* observer) {
  base::AutoLock scoped_lock(lock_);

  if (observers_.count(extension) == 1) {
    observers_[extension]->RemoveObserver(observer);
  }
}

// Extension*
bool ActivityLog::HasObservers(const Extension* extension) const {
  base::AutoLock scoped_lock(lock_);

  // We also return true if extension activity logging is enabled since in that
  // case this class is observing all extensions.
  return observers_.count(extension) > 0 || log_activity_to_stdout_;
}

void ActivityLog::Log(const Extension* extension,
                      Activity activity,
                      const std::string& message) const {
  std::vector<std::string> messages(1, message);
  Log(extension, activity, messages);
}

void ActivityLog::Log(const Extension* extension,
                      Activity activity,
                      const std::vector<std::string>& messages) const {
  base::AutoLock scoped_lock(lock_);

  ObserverMap::const_iterator iter = observers_.find(extension);
  if (iter != observers_.end()) {
    iter->second->Notify(&Observer::OnExtensionActivity, extension, activity,
                         messages);
  }

  if (log_activity_to_stdout_) {
    LOG(INFO) << extension->id() << ":" << ActivityToString(activity) << ":" <<
        JoinString(messages, ' ');
  }
}

void ActivityLog::OnScriptsExecuted(
    const content::WebContents* web_contents,
    const ExecutingScriptsMap& extension_ids,
    int32 on_page_id,
    const GURL& on_url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  const ExtensionService* extension_service =
      ExtensionSystem::Get(profile)->extension_service();
  const ExtensionSet* extensions = extension_service->extensions();

  for (ExecutingScriptsMap::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    const Extension* extension = extensions->GetByID(it->first);
    if (!extension || !HasObservers(extension))
      continue;

    for (std::set<std::string>::const_iterator it2 = it->second.begin();
         it2 != it->second.end(); ++it2) {
      std::vector<std::string> messages;
      messages.push_back(on_url.spec());
      messages.push_back(*it2);
      Log(extension, ActivityLog::ACTIVITY_CONTENT_SCRIPT, messages);
    }
  }
}

// static
const char* ActivityLog::ActivityToString(Activity activity) {
  switch (activity) {
    case ActivityLog::ACTIVITY_EXTENSION_API_CALL:
      return "api_call";
    case ActivityLog::ACTIVITY_EXTENSION_API_BLOCK:
      return "api_block";
    case ActivityLog::ACTIVITY_CONTENT_SCRIPT:
      return "content_script";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace extensions
