// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_preference_helpers.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"

namespace {

const char kIncognitoPersistent[] = "incognito_persistent";
const char kIncognitoSessionOnly[] = "incognito_session_only";
const char kRegular[] = "regular";
const char kRegularOnly[] = "regular_only";

const char kLevelOfControlKey[] = "levelOfControl";

const char kNotControllable[] = "not_controllable";
const char kControlledByOtherExtensions[] = "controlled_by_other_extensions";
const char kControllableByThisExtension[] = "controllable_by_this_extension";
const char kControlledByThisExtension[] = "controlled_by_this_extension";

}  // namespace

namespace extension_preference_helpers {

bool StringToScope(const std::string& s, ExtensionPrefsScope* scope) {
  if (s == kRegular)
    *scope = kExtensionPrefsScopeRegular;
  else if (s == kRegularOnly)
    *scope = kExtensionPrefsScopeRegularOnly;
  else if (s == kIncognitoPersistent)
    *scope = kExtensionPrefsScopeIncognitoPersistent;
  else if (s == kIncognitoSessionOnly)
    *scope = kExtensionPrefsScopeIncognitoSessionOnly;
  else
    return false;
  return true;
}

const char* GetLevelOfControl(
    Profile* profile,
    const std::string& extension_id,
    const std::string& browser_pref,
    bool incognito) {
  PrefService* prefs = incognito ? profile->GetOffTheRecordPrefs()
                                 : profile->GetPrefs();
  const PrefService::Preference* pref =
      prefs->FindPreference(browser_pref.c_str());
  CHECK(pref);
  ExtensionPrefs* ep = profile->GetExtensionService()->extension_prefs();

  if (!pref->IsExtensionModifiable())
    return kNotControllable;

  if (ep->DoesExtensionControlPref(extension_id, browser_pref, incognito))
    return kControlledByThisExtension;

  if (ep->CanExtensionControlPref(extension_id, browser_pref, incognito))
    return kControllableByThisExtension;

  return kControlledByOtherExtensions;
}

void DispatchEventToExtensions(
    Profile* profile,
    const std::string& event_name,
    ListValue* args,
    ExtensionAPIPermission::ID permission,
    bool incognito,
    const std::string& browser_pref) {
  ExtensionEventRouter* router = profile->GetExtensionEventRouter();
  if (!router || !router->HasEventListener(event_name))
    return;
  ExtensionService* extension_service = profile->GetExtensionService();
  const ExtensionSet* extensions = extension_service->extensions();
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    std::string extension_id = (*it)->id();
    // TODO(bauerb): Only iterate over registered event listeners.
    if (router->ExtensionHasEventListener(extension_id, event_name) &&
        (*it)->HasAPIPermission(permission) &&
        (!incognito || extension_service->CanCrossIncognito(*it))) {
      // Inject level of control key-value.
      DictionaryValue* dict;
      bool rv = args->GetDictionary(0, &dict);
      DCHECK(rv);
      std::string level_of_control =
          GetLevelOfControl(profile, extension_id, browser_pref, incognito);
      dict->SetString(kLevelOfControlKey, level_of_control);

      std::string json_args;
      base::JSONWriter::Write(args, &json_args);
      router->DispatchEventToExtension(
          extension_id, event_name, json_args, NULL, GURL());
    }
  }
}

}  // namespace extension_preference_helpers
