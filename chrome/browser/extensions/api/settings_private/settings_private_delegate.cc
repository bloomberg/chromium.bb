// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_delegate.h"

#include "base/json/json_reader.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/url_fixer/url_fixer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace extensions {

namespace settings_private = api::settings_private;

SettingsPrivateDelegate::SettingsPrivateDelegate(Profile* profile)
    : profile_(profile) {
}

SettingsPrivateDelegate::~SettingsPrivateDelegate() {
}

scoped_ptr<base::Value> SettingsPrivateDelegate::GetPref(
    const std::string& name) {
  return prefs_util::GetPref(profile_, name)->ToValue();
}

scoped_ptr<base::Value> SettingsPrivateDelegate::GetAllPrefs() {
  scoped_ptr<base::ListValue> prefs(new base::ListValue());

  const TypedPrefMap& keys = prefs_util::GetWhitelistedKeys();
  for (const auto& it : keys) {
    prefs->Append(GetPref(it.first).release());
  }

  return prefs.Pass();
}

bool SettingsPrivateDelegate::SetPref(const std::string& pref_name,
                                      const base::Value* value) {
  PrefService* pref_service =
      prefs_util::FindServiceForPref(profile_, pref_name);

  if (!prefs_util::IsPrefUserModifiable(profile_, pref_name))
    return false;

  const PrefService::Preference* pref =
      pref_service->FindPreference(pref_name.c_str());
  if (!pref)
    return false;

  DCHECK_EQ(pref->GetType(), value->GetType());

  scoped_ptr<base::Value> temp_value;

  switch (pref->GetType()) {
    case base::Value::TYPE_INTEGER: {
      // In JS all numbers are doubles.
      double double_value;
      if (!value->GetAsDouble(&double_value))
        return false;

      int int_value = static_cast<int>(double_value);
      temp_value.reset(new base::FundamentalValue(int_value));
      value = temp_value.get();
      break;
    }
    case base::Value::TYPE_STRING: {
      std::string original;
      if (!value->GetAsString(&original))
        return false;

      if (prefs_util::IsPrefTypeURL(pref_name)) {
        GURL fixed = url_fixer::FixupURL(original, std::string());
        temp_value.reset(new base::StringValue(fixed.spec()));
        value = temp_value.get();
      }
      break;
    }
    case base::Value::TYPE_LIST: {
      // In case we have a List pref we got a JSON string.
      std::string json_string;
      if (!value->GetAsString(&json_string))
        return false;

      temp_value.reset(base::JSONReader::Read(json_string));
      value = temp_value.get();
      if (!value->IsType(base::Value::TYPE_LIST))
        return false;

      break;
    }
    case base::Value::TYPE_BOOLEAN:
    case base::Value::TYPE_DOUBLE:
      break;
    default:
      return false;
  }

  // TODO(orenb): Process setting metrics here (like "ProcessUserMetric" in
  // CoreOptionsHandler).
  pref_service->Set(pref_name.c_str(), *value);
  return true;
}

}  // namespace extensions
