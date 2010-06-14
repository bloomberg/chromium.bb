// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/configuration_policy_provider_mac.h"

#include "base/logging.h"
#include "base/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"

ConfigurationPolicyProviderMac::ConfigurationPolicyProviderMac()
    : preferences_(new MacPreferences()) {
}

ConfigurationPolicyProviderMac::ConfigurationPolicyProviderMac(
    MacPreferences* preferences) : preferences_(preferences) {
}

bool ConfigurationPolicyProviderMac::Provide(ConfigurationPolicyStore* store) {
  bool success = true;
  const PolicyValueMap* mapping = PolicyValueMapping();

  for (PolicyValueMap::const_iterator current = mapping->begin();
       current != mapping->end(); ++current) {
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
        } else {
          success = false;
        }
        break;
      case Value::TYPE_BOOLEAN:
        if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
          bool bool_value = CFBooleanGetValue((CFBooleanRef)value.get());
          store->Apply(current->policy_type,
                       Value::CreateBooleanValue(bool_value));
        } else {
          success = false;
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
          else
            success = false;
        } else {
          success = false;
        }
        break;
      default:
        NOTREACHED();
        return false;
    }
  }

  return success;
}

