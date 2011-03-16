// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_preference_api.h"

#include <map>

#include "base/json/json_writer.h"
#include "base/singleton.h"
#include "base/stl_util-inl.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_proxy_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_type.h"
#include "content/common/notification_service.h"

namespace {

struct PrefMappingEntry {
  const char* extension_pref;
  const char* browser_pref;
  const char* permission;
};

const char kNotControllable[] = "NotControllable";
const char kControlledByOtherExtensions[] = "ControlledByOtherExtensions";
const char kControllableByThisExtension[] = "ControllableByThisExtension";
const char kControlledByThisExtension[] = "ControlledByThisExtension";

const char kIncognito[] = "incognito";
const char kIncognitoSpecific[] = "incognitoSpecific";
const char kLevelOfControl[] = "levelOfControl";
const char kValue[] = "value";

const char kOnPrefChangeFormat[] = "experimental.preferences.%s.onChange";

const char kIncognitoErrorMessage[] =
    "You do not have permission to access incognito preferences.";

const char kPermissionErrorMessage[] =
    "You do not have permission to access the preference '%s'. "
    "Be sure to declare in your manifest what permissions you need.";

PrefMappingEntry kPrefMapping[] = {
  { "blockThirdPartyCookies",
    prefs::kBlockThirdPartyCookies,
    Extension::kContentSettingsPermission
  },
  { "proxy",
    prefs::kProxy,
    Extension::kProxyPermission
  },
};

class IdentityPrefTransformer : public PrefTransformerInterface {
 public:
  IdentityPrefTransformer() { }
  virtual ~IdentityPrefTransformer() { }

  virtual Value* ExtensionToBrowserPref(const Value* extension_pref,
                                        std::string* error) {
    return extension_pref->DeepCopy();
  }

  virtual Value* BrowserToExtensionPref(const Value* browser_pref) {
    return browser_pref->DeepCopy();
  }
};

// Returns a string constant (defined in the API) indicating the level of
// control this extension has over the specified preference.
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

class PrefMapping {
 public:
  static PrefMapping* GetInstance() {
    return Singleton<PrefMapping>::get();
  }

  bool FindBrowserPrefForExtensionPref(const std::string& extension_pref,
                                       std::string* browser_pref,
                                       std::string* permission) {
    std::map<std::string, std::pair<std::string, std::string> >::iterator it =
        mapping_.find(extension_pref);
    if (it != mapping_.end()) {
      *browser_pref = it->second.first;
      *permission = it->second.second;
      return true;
    }
    return false;
  }

  bool FindEventForBrowserPref(const std::string& browser_pref,
                               std::string* event_name,
                               std::string* permission) {
    std::map<std::string, std::pair<std::string, std::string> >::iterator it =
        event_mapping_.find(browser_pref);
    if (it != event_mapping_.end()) {
      *event_name = it->second.first;
      *permission = it->second.second;
      return true;
    }
    return false;
  }

  PrefTransformerInterface* FindTransformerForBrowserPref(
      const std::string& browser_pref) {
    std::map<std::string, PrefTransformerInterface*>::iterator it =
        transformers_.find(browser_pref);
    if (it != transformers_.end())
      return it->second;
    else
      return identity_transformer_.get();
  }

 private:
  friend struct DefaultSingletonTraits<PrefMapping>;

  PrefMapping() {
    identity_transformer_.reset(new IdentityPrefTransformer());
    for (size_t i = 0; i < arraysize(kPrefMapping); ++i) {
      mapping_[kPrefMapping[i].extension_pref] =
          std::make_pair(kPrefMapping[i].browser_pref,
                         kPrefMapping[i].permission);
      std::string event_name =
          base::StringPrintf(kOnPrefChangeFormat,
                             kPrefMapping[i].extension_pref);
      event_mapping_[kPrefMapping[i].browser_pref] =
          std::make_pair(event_name, kPrefMapping[i].permission);
    }
    DCHECK_EQ(arraysize(kPrefMapping), mapping_.size());
    DCHECK_EQ(arraysize(kPrefMapping), event_mapping_.size());
    RegisterPrefTransformer(prefs::kProxy, new ProxyPrefTransformer());
  }

  ~PrefMapping() {
    STLDeleteContainerPairSecondPointers(transformers_.begin(),
                                         transformers_.end());
  }

  void RegisterPrefTransformer(const std::string& browser_pref,
                               PrefTransformerInterface* transformer) {
    DCHECK_EQ(0u, transformers_.count(browser_pref)) <<
        "Trying to register pref transformer for " << browser_pref << " twice";
    transformers_[browser_pref] = transformer;
  }

  // Mapping from extension pref keys to browser pref keys and permissions.
  std::map<std::string, std::pair<std::string, std::string> > mapping_;

  // Mapping from browser pref keys to extension event names and permissions.
  std::map<std::string, std::pair<std::string, std::string> > event_mapping_;

  // Mapping from browser pref keys to transformers.
  std::map<std::string, PrefTransformerInterface*> transformers_;

  scoped_ptr<PrefTransformerInterface> identity_transformer_;

  DISALLOW_COPY_AND_ASSIGN(PrefMapping);
};

}  // namespace

ExtensionPreferenceEventRouter::ExtensionPreferenceEventRouter(
    Profile* profile) : profile_(profile) {
  registrar_.Init(profile_->GetPrefs());
  incognito_registrar_.Init(profile_->GetOffTheRecordPrefs());
  for (size_t i = 0; i < arraysize(kPrefMapping); ++i) {
    registrar_.Add(kPrefMapping[i].browser_pref, this);
    incognito_registrar_.Add(kPrefMapping[i].browser_pref, this);
  }
}

ExtensionPreferenceEventRouter::~ExtensionPreferenceEventRouter() { }

void ExtensionPreferenceEventRouter::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    const std::string* pref_key =
        Details<const std::string>(details).ptr();
    OnPrefChanged(Source<PrefService>(source).ptr(), *pref_key);
  } else {
    NOTREACHED();
  }
}

void ExtensionPreferenceEventRouter::OnPrefChanged(
    PrefService* pref_service,
    const std::string& browser_pref) {
  bool incognito = (pref_service != profile_->GetPrefs());

  std::string event_name;
  std::string permission;
  bool rv = PrefMapping::GetInstance()->FindEventForBrowserPref(
      browser_pref, &event_name, &permission);
  DCHECK(rv);

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  args.Append(dict);
  const PrefService::Preference* pref =
      pref_service->FindPreference(browser_pref.c_str());
  CHECK(pref);
  ExtensionService* extension_service = profile_->GetExtensionService();
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  dict->Set(kValue, transformer->BrowserToExtensionPref(pref->GetValue()));
  if (incognito) {
    ExtensionPrefs* ep = extension_service->extension_prefs();
    dict->Set(
        kIncognitoSpecific,
        Value::CreateBooleanValue(ep->HasIncognitoPrefValue(browser_pref)));
  }

  ExtensionEventRouter* router = profile_->GetExtensionEventRouter();
  if (!router || !router->HasEventListener(event_name))
    return;
  const ExtensionList* extensions = extension_service->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    std::string extension_id = (*it)->id();
    // TODO(bauerb): Only iterate over registered event listeners.
    if (router->ExtensionHasEventListener(extension_id, event_name) &&
        (*it)->HasApiPermission(permission) &&
        (!incognito || extension_service->CanCrossIncognito(*it))) {
      std::string level_of_control =
          GetLevelOfControl(profile_, extension_id, browser_pref, incognito);
      dict->Set(kLevelOfControl, Value::CreateStringValue(level_of_control));

      std::string json_args;
      base::JSONWriter::Write(&args, false, &json_args);

      DispatchEvent(extension_id, event_name, json_args);
    }
  }
}

void ExtensionPreferenceEventRouter::DispatchEvent(
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& json_args) {
  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, event_name, json_args, NULL, GURL());
}

// TODO(battre): Factor out common parts once this is stable.

GetPreferenceFunction::~GetPreferenceFunction() { }

bool GetPreferenceFunction::RunImpl() {
  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  bool incognito = false;
  if (details->HasKey(kIncognito))
    EXTENSION_FUNCTION_VALIDATE(details->GetBoolean(kIncognito, &incognito));

  if (incognito && !include_incognito()) {
    error_ = kIncognitoErrorMessage;
    return false;
  }

  PrefService* prefs = incognito ? profile_->GetOffTheRecordPrefs()
                                 : profile_->GetPrefs();
  std::string browser_pref;
  std::string permission;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
          pref_key, &browser_pref, &permission));
  if (!GetExtension()->HasApiPermission(permission)) {
    error_ = base::StringPrintf(kPermissionErrorMessage, pref_key.c_str());
    return false;
  }

  const PrefService::Preference* pref =
      prefs->FindPreference(browser_pref.c_str());
  CHECK(pref);
  std::string level_of_control =
      GetLevelOfControl(profile_, extension_id(), browser_pref, incognito);

  scoped_ptr<DictionaryValue> result(new DictionaryValue);
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  result->Set(kValue, transformer->BrowserToExtensionPref(pref->GetValue()));
  result->Set(kLevelOfControl, Value::CreateStringValue(level_of_control));
  if (incognito) {
    ExtensionPrefs* ep = profile_->GetExtensionService()->extension_prefs();
    result->Set(
        kIncognitoSpecific,
        Value::CreateBooleanValue(ep->HasIncognitoPrefValue(browser_pref)));
  }
  result_.reset(result.release());
  return true;
}

SetPreferenceFunction::~SetPreferenceFunction() { }

bool SetPreferenceFunction::RunImpl() {
  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  Value* value = NULL;
  EXTENSION_FUNCTION_VALIDATE(details->Get(kValue, &value));

  bool incognito = false;
  if (details->HasKey(kIncognito))
    EXTENSION_FUNCTION_VALIDATE(details->GetBoolean(kIncognito, &incognito));

  if (incognito && !include_incognito()) {
    error_ = kIncognitoErrorMessage;
    return false;
  }

  std::string browser_pref;
  std::string permission;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
          pref_key, &browser_pref, &permission));
  if (!GetExtension()->HasApiPermission(permission)) {
    error_ = base::StringPrintf(kPermissionErrorMessage, pref_key.c_str());
    return false;
  }
  ExtensionPrefs* prefs = profile_->GetExtensionService()->extension_prefs();
  const PrefService::Preference* pref =
      prefs->pref_service()->FindPreference(browser_pref.c_str());
  CHECK(pref);
  EXTENSION_FUNCTION_VALIDATE(value->GetType() == pref->GetType());
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  std::string error;
  Value* browserPrefValue = transformer->ExtensionToBrowserPref(value, &error);
  if (!browserPrefValue) {
    error_ = error;
    return false;
  }
  prefs->SetExtensionControlledPref(extension_id(),
                                    browser_pref,
                                    incognito,
                                    browserPrefValue);
  return true;
}

ClearPreferenceFunction::~ClearPreferenceFunction() { }

bool ClearPreferenceFunction::RunImpl() {
  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  bool incognito = false;
  if (details->HasKey(kIncognito))
    EXTENSION_FUNCTION_VALIDATE(details->GetBoolean(kIncognito, &incognito));

  // We don't check incognito permissions here, as an extension should be always
  // allowed to clear its own settings.

  std::string browser_pref;
  std::string permission;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
          pref_key, &browser_pref, &permission));
  if (!GetExtension()->HasApiPermission(permission)) {
    error_ = base::StringPrintf(kPermissionErrorMessage, pref_key.c_str());
    return false;
  }
  ExtensionPrefs* prefs = profile_->GetExtensionService()->extension_prefs();
  prefs->RemoveExtensionControlledPref(extension_id(), browser_pref, incognito);
  return true;
}
