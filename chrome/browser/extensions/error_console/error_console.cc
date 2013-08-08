// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include <algorithm>

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/error_console/extension_error.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/constants.h"

namespace extensions {

void ErrorConsole::Observer::OnErrorConsoleDestroyed() {
}

ErrorConsole::ErrorConsole(Profile* profile) : profile_(profile) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

ErrorConsole::~ErrorConsole() {
  FOR_EACH_OBSERVER(Observer, observers_, OnErrorConsoleDestroyed());
}

// static
ErrorConsole* ErrorConsole::Get(Profile* profile) {
  return ExtensionSystem::Get(profile)->error_console();
}

void ErrorConsole::ReportError(scoped_ptr<ExtensionError> error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  errors_.push_back(error.release());
  FOR_EACH_OBSERVER(Observer, observers_, OnErrorAdded(errors_.back()));
}

ErrorConsole::WeakErrorList ErrorConsole::GetErrorsForExtension(
    const std::string& extension_id) const {
  WeakErrorList result;
  for (ErrorList::const_iterator iter = errors_.begin();
       iter != errors_.end(); ++iter) {
    if ((*iter)->extension_id() == extension_id)
      result.push_back(*iter);
  }
  return result;
}

void ErrorConsole::RemoveError(const ExtensionError* error) {
  ErrorList::iterator iter = std::find(errors_.begin(), errors_.end(), error);
  CHECK(iter != errors_.end());
  errors_.erase(iter);
}

void ErrorConsole::RemoveAllErrors() {
  errors_.clear();
}

void ErrorConsole::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void ErrorConsole::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void ErrorConsole::RemoveIncognitoErrors() {
  WeakErrorList to_remove;
  for (ErrorList::const_iterator iter = errors_.begin();
       iter != errors_.end(); ++iter) {
    if ((*iter)->from_incognito())
      to_remove.push_back(*iter);
  }

  for (WeakErrorList::const_iterator iter = to_remove.begin();
       iter != to_remove.end(); ++iter) {
    RemoveError(*iter);
  }
}

void ErrorConsole::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      // If incognito profile which we are associated with is destroyed, also
      // destroy all incognito errors.
      if (profile->IsOffTheRecord() && profile_->IsSameProfile(profile))
        RemoveIncognitoErrors();
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace extensions
