// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include <list>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
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

ErrorConsole::ErrorConsole(Profile* profile) : enabled_(false),
                                               profile_(profile) {
// TODO(rdevlin.cronin): Remove once crbug.com/159265 is fixed.
#if !defined(ENABLE_EXTENSIONS)
  return;
#endif

  // If we don't have the necessary FeatureSwitch enabled, then return
  // immediately. Since we never register for any notifications, this ensures
  // the ErrorConsole will never be enabled.
  if (!FeatureSwitch::error_console()->IsEnabled())
    return;

  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(prefs::kExtensionsUIDeveloperMode,
                      base::Bind(&ErrorConsole::OnPrefChanged,
                                 base::Unretained(this)));

  if (profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode))
    Enable();
}

ErrorConsole::~ErrorConsole() {
  FOR_EACH_OBSERVER(Observer, observers_, OnErrorConsoleDestroyed());
  RemoveAllErrors();
}

// static
ErrorConsole* ErrorConsole::Get(Profile* profile) {
  return ExtensionSystem::Get(profile)->error_console();
}

void ErrorConsole::ReportError(scoped_ptr<ExtensionError> error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!enabled_ || !Extension::IdIsValid(error->extension_id()))
    return;

  ErrorList* extension_errors = &errors_[error->extension_id()];

  // First, check if it's a duplicate.
  for (ErrorList::iterator iter = extension_errors->begin();
       iter != extension_errors->end(); ++iter) {
    // If we find a duplicate error, remove the old error and add the new one,
    // incrementing the occurrence count of the error. We use the new error
    // for runtime errors, so we can link to the latest context, inspectable
    // view, etc.
    if (error->IsEqual(*iter)) {
      error->set_occurrences((*iter)->occurrences() + 1);
      delete *iter;
      extension_errors->erase(iter);
      break;
    }
  }

  // If there are too many errors for an extension already, limit ourselves to
  // the most recent ones.
  if (extension_errors->size() >= kMaxErrorsPerExtension) {
    delete extension_errors->front();
    extension_errors->pop_front();
  }

  extension_errors->push_back(error.release());

  FOR_EACH_OBSERVER(
      Observer, observers_, OnErrorAdded(extension_errors->back()));
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

void ErrorConsole::OnPrefChanged() {
  bool developer_mode =
      profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode);

  if (developer_mode && !enabled_)
    Enable();
  else if (!developer_mode && enabled_)
    Disable();
}

void ErrorConsole::Enable() {
  enabled_ = true;

  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::NotificationService::AllBrowserContextsAndSources());
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::Source<Profile>(profile_));
}

void ErrorConsole::Disable() {
  notification_registrar_.RemoveAll();
  RemoveAllErrors();
  enabled_ = false;
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
