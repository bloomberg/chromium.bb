// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider_mac.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/sys_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/preferences_mac.h"
#include "chrome/common/chrome_paths.h"
#include "policy/policy_constants.h"

using base::mac::CFCast;
using base::mac::ScopedCFTypeRef;

namespace policy {

namespace {

FilePath GetManagedPolicyPath() {
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

// Callback function for CFDictionaryApplyFunction. |key| and |value| are an
// entry of the CFDictionary that should be converted into an equivalent entry
// in the DictionaryValue in |context|.
void DictionaryEntryToValue(const void* key, const void* value, void* context) {
  if (CFStringRef cf_key = CFCast<CFStringRef>(key)) {
    base::Value* converted =
        MacPreferencesPolicyProviderDelegate::CreateValueFromProperty(
            static_cast<CFPropertyListRef>(value));
    if (converted) {
      const std::string string = base::SysCFStringRefToUTF8(cf_key);
      static_cast<base::DictionaryValue *>(context)->Set(string, converted);
    }
  }
}

// Callback function for CFArrayApplyFunction. |value| is an entry of the
// CFArray that should be converted into an equivalent entry in the ListValue
// in |context|.
void ArrayEntryToValue(const void* value, void* context) {
  base::Value* converted =
      MacPreferencesPolicyProviderDelegate::CreateValueFromProperty(
          static_cast<CFPropertyListRef>(value));
  if (converted)
    static_cast<base::ListValue *>(context)->Append(converted);
}

}  // namespace

MacPreferencesPolicyProviderDelegate::MacPreferencesPolicyProviderDelegate(
    MacPreferences* preferences,
    const PolicyDefinitionList* policy_list,
    PolicyLevel level)
    : FileBasedPolicyProvider::ProviderDelegate(GetManagedPolicyPath()),
      policy_list_(policy_list),
      preferences_(preferences),
      level_(level) {}

MacPreferencesPolicyProviderDelegate::~MacPreferencesPolicyProviderDelegate() {}

PolicyMap* MacPreferencesPolicyProviderDelegate::Load() {
  preferences_->AppSynchronize(kCFPreferencesCurrentApplication);
  PolicyMap* policies = new PolicyMap;

  const PolicyDefinitionList::Entry* current;
  for (current = policy_list_->begin; current != policy_list_->end; ++current) {
    base::mac::ScopedCFTypeRef<CFStringRef> name(
        base::SysUTF8ToCFStringRef(current->name));
    base::mac::ScopedCFTypeRef<CFPropertyListRef> value(
        preferences_->CopyAppValue(name, kCFPreferencesCurrentApplication));
    if (!value.get())
      continue;
    bool forced =
        preferences_->AppValueIsForced(name, kCFPreferencesCurrentApplication);
    PolicyLevel level = forced ? POLICY_LEVEL_MANDATORY :
                                 POLICY_LEVEL_RECOMMENDED;
    if (level != level_)
      continue;

    // TODO(joaodasilva): figure the policy scope.

    base::Value* policy = CreateValueFromProperty(value);
    if (policy)
      policies->Set(current->name, level_, POLICY_SCOPE_USER, policy);
  }

  return policies;
}

base::Time MacPreferencesPolicyProviderDelegate::GetLastModification() {
  base::PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(config_file_path(), &file_info) ||
      file_info.is_directory) {
    return base::Time();
  }

  return file_info.last_modified;
}

// static
base::Value* MacPreferencesPolicyProviderDelegate::CreateValueFromProperty(
    CFPropertyListRef property) {
  if (CFCast<CFNullRef>(property))
    return base::Value::CreateNullValue();

  if (CFBooleanRef boolean = CFCast<CFBooleanRef>(property))
    return base::Value::CreateBooleanValue(CFBooleanGetValue(boolean));

  if (CFNumberRef number = CFCast<CFNumberRef>(property)) {
    // CFNumberGetValue() converts values implicitly when the conversion is not
    // lossy. Check the type before trying to convert.
    if (CFNumberIsFloatType(number)) {
      double double_value;
      if (CFNumberGetValue(number, kCFNumberDoubleType, &double_value))
        return base::Value::CreateDoubleValue(double_value);
    } else {
      int int_value;
      if (CFNumberGetValue(number, kCFNumberIntType, &int_value))
        return base::Value::CreateIntegerValue(int_value);
    }
  }

  if (CFStringRef string = CFCast<CFStringRef>(property))
    return base::Value::CreateStringValue(base::SysCFStringRefToUTF8(string));

  if (CFDictionaryRef dict = CFCast<CFDictionaryRef>(property)) {
    base::DictionaryValue* dict_value = new base::DictionaryValue();
    CFDictionaryApplyFunction(dict, DictionaryEntryToValue, dict_value);
    return dict_value;
  }

  if (CFArrayRef array = CFCast<CFArrayRef>(property)) {
    base::ListValue* list_value = new base::ListValue();
    CFArrayApplyFunction(array,
                         CFRangeMake(0, CFArrayGetCount(array)),
                         ArrayEntryToValue,
                         list_value);
    return list_value;
  }

  return NULL;
}

ConfigurationPolicyProviderMac::ConfigurationPolicyProviderMac(
    const PolicyDefinitionList* policy_list,
    PolicyLevel level)
    : FileBasedPolicyProvider(
          policy_list,
          new MacPreferencesPolicyProviderDelegate(new MacPreferences,
                                                   policy_list,
                                                   level)) {}

ConfigurationPolicyProviderMac::ConfigurationPolicyProviderMac(
    const PolicyDefinitionList* policy_list,
    PolicyLevel level,
    MacPreferences* preferences)
    : FileBasedPolicyProvider(
          policy_list,
          new MacPreferencesPolicyProviderDelegate(preferences,
                                                   policy_list,
                                                   level)) {}

}  // namespace policy
