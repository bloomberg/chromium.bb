// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_preference_api.h"

#include <map>

#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_preference_api_constants.h"
#include "chrome/browser/extensions/extension_preference_helpers.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_prefs_scope.h"
#include "chrome/browser/extensions/extension_proxy_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace {

struct PrefMappingEntry {
  const char* extension_pref;
  const char* browser_pref;
  ExtensionAPIPermission::ID permission;
};

const char kNotControllable[] = "not_controllable";
const char kControlledByOtherExtensions[] = "controlled_by_other_extensions";
const char kControllableByThisExtension[] = "controllable_by_this_extension";
const char kControlledByThisExtension[] = "controlled_by_this_extension";

const char kIncognitoSpecific[] = "incognitoSpecific";
const char kLevelOfControl[] = "levelOfControl";
const char kValue[] = "value";

const char kOnPrefChangeFormat[] = "types.ChromeSetting.%s.onChange";

PrefMappingEntry kPrefMapping[] = {
  { "alternateErrorPagesEnabled",
    prefs::kAlternateErrorPagesEnabled,
    ExtensionAPIPermission::kExperimental
  },
  { "autofillEnabled",
    prefs::kAutofillEnabled,
    ExtensionAPIPermission::kExperimental
  },
  { "hyperlinkAuditingEnabled",
    prefs::kEnableHyperlinkAuditing,
    ExtensionAPIPermission::kExperimental
  },
  { "instantEnabled",
    prefs::kInstantEnabled,
    ExtensionAPIPermission::kExperimental
  },
  // TODO(mkwst): come back to this once the UMA discussion has been resolved.
  // { "metricsReportingEnabled",
  //   prefs::kMetricsReportingEnabled,
  //   ExtensionAPIPermission::kMetrics
  // },
  { "networkPredictionEnabled",
    prefs::kNetworkPredictionEnabled,
    ExtensionAPIPermission::kExperimental
  },
  { "proxy",
    prefs::kProxy,
    ExtensionAPIPermission::kProxy
  },
  { "referrersEnabled",
    prefs::kEnableReferrers,
    ExtensionAPIPermission::kExperimental
  },
  { "searchSuggestEnabled",
    prefs::kSearchSuggestEnabled,
    ExtensionAPIPermission::kExperimental
  },
  { "safeBrowsingEnabled",
    prefs::kSafeBrowsingEnabled,
    ExtensionAPIPermission::kExperimental
  },
  { "thirdPartyCookiesAllowed",
    prefs::kBlockThirdPartyCookies,
    ExtensionAPIPermission::kExperimental
  },
  { "translationServiceEnabled",
    prefs::kEnableTranslate,
    ExtensionAPIPermission::kExperimental
  }
};

class IdentityPrefTransformer : public PrefTransformerInterface {
 public:
  virtual Value* ExtensionToBrowserPref(const Value* extension_pref,
                                        std::string* error,
                                        bool* bad_message) {
    return extension_pref->DeepCopy();
  }

  virtual Value* BrowserToExtensionPref(const Value* browser_pref) {
    return browser_pref->DeepCopy();
  }
};

class InvertBooleanTransformer : public PrefTransformerInterface {
 public:
  virtual Value* ExtensionToBrowserPref(const Value* extension_pref,
                                        std::string* error,
                                        bool* bad_message) {
    return InvertBooleanValue(extension_pref);
  }

  virtual Value* BrowserToExtensionPref(const Value* browser_pref) {
    return InvertBooleanValue(browser_pref);
  }

 private:
  static Value* InvertBooleanValue(const Value* value) {
    bool bool_value = false;
    bool result = value->GetAsBoolean(&bool_value);
    DCHECK(result);
    return Value::CreateBooleanValue(!bool_value);
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
                                       ExtensionAPIPermission::ID* permission) {
    PrefMap::iterator it = mapping_.find(extension_pref);
    if (it != mapping_.end()) {
      *browser_pref = it->second.first;
      *permission = it->second.second;
      return true;
    }
    return false;
  }

  bool FindEventForBrowserPref(const std::string& browser_pref,
                               std::string* event_name,
                               ExtensionAPIPermission::ID* permission) {
    PrefMap::iterator it = event_mapping_.find(browser_pref);
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
    RegisterPrefTransformer(prefs::kBlockThirdPartyCookies,
                            new InvertBooleanTransformer());
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

  typedef std::map<std::string,
                   std::pair<std::string, ExtensionAPIPermission::ID> >
          PrefMap;

  // Mapping from extension pref keys to browser pref keys and permissions.
  PrefMap mapping_;

  // Mapping from browser pref keys to extension event names and permissions.
  PrefMap event_mapping_;

  // Mapping from browser pref keys to transformers.
  std::map<std::string, PrefTransformerInterface*> transformers_;

  scoped_ptr<PrefTransformerInterface> identity_transformer_;

  DISALLOW_COPY_AND_ASSIGN(PrefMapping);
};

}  // namespace

namespace keys = extension_preference_api_constants;
namespace helpers = extension_preference_helpers;

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
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    const std::string* pref_key =
        content::Details<const std::string>(details).ptr();
    OnPrefChanged(content::Source<PrefService>(source).ptr(), *pref_key);
  } else {
    NOTREACHED();
  }
}

void ExtensionPreferenceEventRouter::OnPrefChanged(
    PrefService* pref_service,
    const std::string& browser_pref) {
  bool incognito = (pref_service != profile_->GetPrefs());

  std::string event_name;
  ExtensionAPIPermission::ID permission = ExtensionAPIPermission::kInvalid;
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
    dict->SetBoolean(kIncognitoSpecific,
                     ep->HasIncognitoPrefValue(browser_pref));
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
        (*it)->HasAPIPermission(permission) &&
        (!incognito || extension_service->CanCrossIncognito(*it))) {
      std::string level_of_control =
          GetLevelOfControl(profile_, extension_id, browser_pref, incognito);
      dict->SetString(kLevelOfControl, level_of_control);

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
  if (details->HasKey(keys::kIncognitoKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetBoolean(keys::kIncognitoKey,
                                                    &incognito));

  if (incognito && !include_incognito()) {
    error_ = keys::kIncognitoErrorMessage;
    return false;
  }

  PrefService* prefs = incognito ? profile_->GetOffTheRecordPrefs()
                                 : profile_->GetPrefs();
  std::string browser_pref;
  ExtensionAPIPermission::ID permission = ExtensionAPIPermission::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
          pref_key, &browser_pref, &permission));
  if (!GetExtension()->HasAPIPermission(permission)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kPermissionErrorMessage, pref_key);
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
  result->SetString(kLevelOfControl, level_of_control);
  if (incognito) {
    ExtensionPrefs* ep = profile_->GetExtensionService()->extension_prefs();
    result->SetBoolean(kIncognitoSpecific,
                       ep->HasIncognitoPrefValue(browser_pref));
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

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  if (details->HasKey(keys::kScopeKey)) {
    std::string scope_str;
    EXTENSION_FUNCTION_VALIDATE(
        details->GetString(keys::kScopeKey, &scope_str));

    EXTENSION_FUNCTION_VALIDATE(helpers::StringToScope(scope_str, &scope));
  }

  bool incognito = (scope == kExtensionPrefsScopeIncognitoPersistent ||
                    scope == kExtensionPrefsScopeIncognitoSessionOnly);
  if (incognito) {
    // Regular profiles can't access incognito unless include_incognito is true.
    if (!profile()->IsOffTheRecord() && !include_incognito()) {
      error_ = keys::kIncognitoErrorMessage;
      return false;
    }
  } else {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    if (profile()->IsOffTheRecord()) {
      error_ = "Can't modify regular settings from an incognito context.";
      return false;
    }
  }

  if (scope == kExtensionPrefsScopeIncognitoSessionOnly &&
      !profile_->HasOffTheRecordProfile()) {
    error_ = keys::kIncognitoSessionOnlyErrorMessage;
    return false;
  }

  std::string browser_pref;
  ExtensionAPIPermission::ID permission = ExtensionAPIPermission::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
          pref_key, &browser_pref, &permission));
  if (!GetExtension()->HasAPIPermission(permission)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kPermissionErrorMessage, pref_key);
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
  bool bad_message = false;
  Value* browserPrefValue =
      transformer->ExtensionToBrowserPref(value, &error, &bad_message);
  if (!browserPrefValue) {
    error_ = error;
    bad_message_ = bad_message;
    return false;
  }
  prefs->SetExtensionControlledPref(extension_id(),
                                    browser_pref,
                                    scope,
                                    browserPrefValue);
  return true;
}

ClearPreferenceFunction::~ClearPreferenceFunction() { }

bool ClearPreferenceFunction::RunImpl() {
  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  if (details->HasKey(keys::kScopeKey)) {
    std::string scope_str;
    EXTENSION_FUNCTION_VALIDATE(
        details->GetString(keys::kScopeKey, &scope_str));

    EXTENSION_FUNCTION_VALIDATE(helpers::StringToScope(scope_str, &scope));
  }

  bool incognito = (scope == kExtensionPrefsScopeIncognitoPersistent ||
                    scope == kExtensionPrefsScopeIncognitoSessionOnly);
  if (incognito) {
    // We don't check incognito permissions here, as an extension should be
    // always allowed to clear its own settings.
  } else {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    if (profile()->IsOffTheRecord()) {
      error_ = "Can't modify regular settings from an incognito context.";
      return false;
    }
  }

  std::string browser_pref;
  ExtensionAPIPermission::ID permission = ExtensionAPIPermission::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
          pref_key, &browser_pref, &permission));
  if (!GetExtension()->HasAPIPermission(permission)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kPermissionErrorMessage, pref_key);
    return false;
  }
  ExtensionPrefs* prefs = profile_->GetExtensionService()->extension_prefs();
  prefs->RemoveExtensionControlledPref(extension_id(), browser_pref, scope);
  return true;
}
