// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include <list>

#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/constants.h"

namespace extensions {

namespace {

const size_t kMaxErrorsPerExtension = 100;

// Iterate through an error list and remove and delete all errors which were
// from an incognito context.
void DeleteIncognitoErrorsFromList(ErrorConsole::ErrorList* list) {
  ErrorConsole::ErrorList::iterator iter = list->begin();
  while (iter != list->end()) {
    if ((*iter)->from_incognito()) {
      delete *iter;
      iter = list->erase(iter);
    } else {
      ++iter;
    }
  }
}

base::LazyInstance<ErrorConsole::ErrorList> g_empty_error_list =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

void ErrorConsole::Observer::OnErrorConsoleDestroyed() {
}

ErrorConsole::ErrorConsole(Profile* profile) : profile_(profile) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
}

ErrorConsole::~ErrorConsole() {
  FOR_EACH_OBSERVER(Observer, observers_, OnErrorConsoleDestroyed());
  RemoveAllErrors();
}

// static
ErrorConsole* ErrorConsole::Get(Profile* profile) {
  return ExtensionSystem::Get(profile)->error_console();
}

void ErrorConsole::ReportError(scoped_ptr<const ExtensionError> scoped_error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const ExtensionError* error = scoped_error.release();
  // If there are too many errors for an extension already, limit ourselves to
  // the most recent ones.
  ErrorList* error_list = &errors_[error->extension_id()];
  if (error_list->size() >= kMaxErrorsPerExtension) {
    delete error_list->front();
    error_list->pop_front();
  }
  error_list->push_back(error);

  FOR_EACH_OBSERVER(Observer, observers_, OnErrorAdded(error));
}

const ErrorConsole::ErrorList& ErrorConsole::GetErrorsForExtension(
    const std::string& extension_id) const {
  ErrorMap::const_iterator iter = errors_.find(extension_id);
  if (iter != errors_.end())
    return iter->second;
  return g_empty_error_list.Get();
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
  for (ErrorMap::iterator iter = errors_.begin();
       iter != errors_.end(); ++iter) {
    DeleteIncognitoErrorsFromList(&(iter->second));
  }
}

void ErrorConsole::RemoveErrorsForExtension(const std::string& extension_id) {
  ErrorMap::iterator iter = errors_.find(extension_id);
  if (iter != errors_.end()) {
    STLDeleteContainerPointers(iter->second.begin(), iter->second.end());
    errors_.erase(iter);
  }
}

void ErrorConsole::RemoveAllErrors() {
  for (ErrorMap::iterator iter = errors_.begin(); iter != errors_.end(); ++iter)
    STLDeleteContainerPointers(iter->second.begin(), iter->second.end());
  errors_.clear();
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
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
      // No need to check the profile here, since we registered to only receive
      // notifications from our own.
      RemoveErrorsForExtension(
          content::Details<Extension>(details).ptr()->id());
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace extensions
