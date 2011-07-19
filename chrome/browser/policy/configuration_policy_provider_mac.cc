// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider_mac.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/path_service.h"
#include "base/sys_string_conversions.h"
#include "chrome/common/chrome_paths.h"

namespace policy {

static FilePath GetManagedPolicyPath() {
  // This constructs the path to the plist file in which Mac OS X stores the
  // managed preference for the application. This is undocumented and therefore
  // fragile, but if it doesn't work out, FileBasedPolicyLoader has a task that
  // polls periodically in order to reload managed preferences later even if we
  // missed the change.
  FilePath path;
  if (!PathService::Get(chrome::DIR_MANAGED_PREFS, &path))
    return FilePath();

  CFBundleRef bundle(CFBundleGetMainBundle());
  if (!bundle)
    return FilePath();

  CFStringRef bundle_id = CFBundleGetIdentifier(bundle);
  if (!bundle_id)
    return FilePath();

  return path.Append(base::SysCFStringRefToUTF8(bundle_id) + ".plist");
}

MacPreferencesPolicyProviderDelegate::MacPreferencesPolicyProviderDelegate(
    MacPreferences* preferences,
    const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list)
    : FileBasedPolicyProvider::ProviderDelegate(GetManagedPolicyPath()),
      policy_list_(policy_list),
      preferences_(preferences) {
}

MacPreferencesPolicyProviderDelegate::~MacPreferencesPolicyProviderDelegate() {}

DictionaryValue* MacPreferencesPolicyProviderDelegate::Load() {
  preferences_->AppSynchronize(kCFPreferencesCurrentApplication);
  DictionaryValue* policy = new DictionaryValue;

  const ConfigurationPolicyProvider::PolicyDefinitionList::Entry* current;
  for (current = policy_list_->begin; current != policy_list_->end; ++current) {
    base::mac::ScopedCFTypeRef<CFStringRef> name(
        base::SysUTF8ToCFStringRef(current->name));
    base::mac::ScopedCFTypeRef<CFPropertyListRef> value(
        preferences_->CopyAppValue(name, kCFPreferencesCurrentApplication));
    if (!value.get())
      continue;
    if (!preferences_->AppValueIsForced(name, kCFPreferencesCurrentApplication))
      continue;

    switch (current->value_type) {
      case Value::TYPE_STRING:
        if (CFGetTypeID(value) == CFStringGetTypeID()) {
          std::string string_value =
              base::SysCFStringRefToUTF8((CFStringRef)value.get());
          policy->SetString(current->name, string_value);
        }
        break;
      case Value::TYPE_BOOLEAN:
        if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
          bool bool_value = CFBooleanGetValue((CFBooleanRef)value.get());
          policy->SetBoolean(current->name, bool_value);
        }
        break;
      case Value::TYPE_INTEGER:
        if (CFGetTypeID(value) == CFNumberGetTypeID()) {
          int int_value;
          bool cast = CFNumberGetValue((CFNumberRef)value.get(),
                                       kCFNumberIntType,
                                       &int_value);
          if (cast)
            policy->SetInteger(current->name, int_value);
        }
        break;
      case Value::TYPE_LIST:
        if (CFGetTypeID(value) == CFArrayGetTypeID()) {
          scoped_ptr<ListValue> list_value(new ListValue);
          bool valid_array = true;
          CFArrayRef array_value = (CFArrayRef)value.get();
          for (CFIndex i = 0; i < CFArrayGetCount(array_value); ++i) {
            // For now we assume that all values are strings.
            CFStringRef array_string =
                (CFStringRef)CFArrayGetValueAtIndex(array_value, i);
            if (CFGetTypeID(array_string) == CFStringGetTypeID()) {
              std::string array_string_value =
                  base::SysCFStringRefToUTF8(array_string);
              list_value->Append(Value::CreateStringValue(array_string_value));
            } else {
              valid_array = false;
            }
          }
          if (valid_array)
            policy->Set(current->name, list_value.release());
        }
        break;
      default:
        NOTREACHED();
    }
  }

  return policy;
}

base::Time MacPreferencesPolicyProviderDelegate::GetLastModification() {
  base::PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(config_file_path(), &file_info) ||
      file_info.is_directory) {
    return base::Time();
  }

  return file_info.last_modified;
}

ConfigurationPolicyProviderMac::ConfigurationPolicyProviderMac(
    const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list)
    : FileBasedPolicyProvider(policy_list,
          new MacPreferencesPolicyProviderDelegate(new MacPreferences,
                                                   policy_list)) {
}

ConfigurationPolicyProviderMac::ConfigurationPolicyProviderMac(
    const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list,
    MacPreferences* preferences)
    : FileBasedPolicyProvider(policy_list,
          new MacPreferencesPolicyProviderDelegate(preferences,
                                                   policy_list)) {
}

}  // namespace policy
