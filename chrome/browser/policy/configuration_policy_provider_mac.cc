// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider_mac.h"

#include "base/logging.h"
#include "base/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"

namespace policy {

ConfigurationPolicyProviderMac::ConfigurationPolicyProviderMac(
    const StaticPolicyValueMap& policy_map)
    : ConfigurationPolicyProvider(policy_map),
      preferences_(new MacPreferences()) {
}

ConfigurationPolicyProviderMac::ConfigurationPolicyProviderMac(
    const StaticPolicyValueMap& policy_map, MacPreferences* preferences)
    : ConfigurationPolicyProvider(policy_map), preferences_(preferences) {
}

bool ConfigurationPolicyProviderMac::Provide(ConfigurationPolicyStore* store) {
  const PolicyValueMap& mapping = policy_value_map();

  for (PolicyValueMap::const_iterator current = mapping.begin();
       current != mapping.end(); ++current) {
    scoped_cftyperef<CFStringRef> name(
        base::SysUTF8ToCFStringRef(current->name));
    scoped_cftyperef<CFPropertyListRef> value(
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
          store->Apply(
              current->policy_type,
              Value::CreateStringValue(string_value));
        }
        break;
      case Value::TYPE_BOOLEAN:
        if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
          bool bool_value = CFBooleanGetValue((CFBooleanRef)value.get());
          store->Apply(current->policy_type,
                       Value::CreateBooleanValue(bool_value));
        }
        break;
      case Value::TYPE_INTEGER:
        if (CFGetTypeID(value) == CFNumberGetTypeID()) {
          int int_value;
          bool cast = CFNumberGetValue((CFNumberRef)value.get(),
                                       kCFNumberIntType,
                                       &int_value);
          if (cast)
            store->Apply(current->policy_type,
                         Value::CreateIntegerValue(int_value));
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
            store->Apply(current->policy_type, list_value.release());
        }
        break;
      default:
        NOTREACHED();
        return false;
    }
  }

  return true;
}

}  // namespace policy
