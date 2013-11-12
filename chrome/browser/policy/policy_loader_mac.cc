// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_loader_mac.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_load_status.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/preferences_mac.h"
#include "chrome/browser/policy/schema_map.h"
#include "components/policy/core/common/schema.h"
#include "policy/policy_constants.h"

using base::mac::CFCast;
using base::ScopedCFTypeRef;

namespace policy {

namespace {

// Callback function for CFDictionaryApplyFunction. |key| and |value| are an
// entry of the CFDictionary that should be converted into an equivalent entry
// in the DictionaryValue in |context|.
void DictionaryEntryToValue(const void* key, const void* value, void* context) {
  if (CFStringRef cf_key = CFCast<CFStringRef>(key)) {
    base::Value* converted =
        PolicyLoaderMac::CreateValueFromProperty(
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
      PolicyLoaderMac::CreateValueFromProperty(
          static_cast<CFPropertyListRef>(value));
  if (converted)
    static_cast<base::ListValue *>(context)->Append(converted);
}

}  // namespace

PolicyLoaderMac::PolicyLoaderMac(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::FilePath& managed_policy_path,
    MacPreferences* preferences)
    : AsyncPolicyLoader(task_runner),
      preferences_(preferences),
      managed_policy_path_(managed_policy_path) {}

PolicyLoaderMac::~PolicyLoaderMac() {}

void PolicyLoaderMac::InitOnBackgroundThread() {
  if (!managed_policy_path_.empty()) {
    watcher_.Watch(
        managed_policy_path_, false,
        base::Bind(&PolicyLoaderMac::OnFileUpdated, base::Unretained(this)));
  }
}

scoped_ptr<PolicyBundle> PolicyLoaderMac::Load() {
  preferences_->AppSynchronize(kCFPreferencesCurrentApplication);
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());

  // Load Chrome's policy.
  PolicyMap& chrome_policy =
      bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));

  PolicyLoadStatusSample status;
  bool policy_present = false;
  const Schema* schema =
      schema_map()->GetSchema(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));
  for (Schema::Iterator it = schema->GetPropertiesIterator();
       !it.IsAtEnd(); it.Advance()) {
    base::ScopedCFTypeRef<CFStringRef> name(
        base::SysUTF8ToCFStringRef(it.key()));
    base::ScopedCFTypeRef<CFPropertyListRef> value(
        preferences_->CopyAppValue(name, kCFPreferencesCurrentApplication));
    if (!value.get())
      continue;
    policy_present = true;
    bool forced =
        preferences_->AppValueIsForced(name, kCFPreferencesCurrentApplication);
    PolicyLevel level = forced ? POLICY_LEVEL_MANDATORY :
                                 POLICY_LEVEL_RECOMMENDED;
    // TODO(joaodasilva): figure the policy scope.
    base::Value* policy = CreateValueFromProperty(value);
    if (policy)
      chrome_policy.Set(it.key(), level, POLICY_SCOPE_USER, policy, NULL);
    else
      status.Add(POLICY_LOAD_STATUS_PARSE_ERROR);
  }

  if (!policy_present)
    status.Add(POLICY_LOAD_STATUS_NO_POLICY);

  // Load policy for the registered components.
  LoadPolicyForDomain(POLICY_DOMAIN_EXTENSIONS, "extensions", bundle.get());

  return bundle.Pass();
}

base::Time PolicyLoaderMac::LastModificationTime() {
  base::PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(managed_policy_path_, &file_info) ||
      file_info.is_directory) {
    return base::Time();
  }

  return file_info.last_modified;
}

// static
base::Value* PolicyLoaderMac::CreateValueFromProperty(
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

void PolicyLoaderMac::LoadPolicyForDomain(
    PolicyDomain domain,
    const std::string& domain_name,
    PolicyBundle* bundle) {
  std::string id_prefix(base::mac::BaseBundleID());
  id_prefix.append(".").append(domain_name).append(".");

  const ComponentMap* components = schema_map()->GetComponents(domain);
  if (!components)
    return;

  for (ComponentMap::const_iterator it = components->begin();
       it != components->end(); ++it) {
    PolicyMap policy;
    LoadPolicyForComponent(id_prefix + it->first, it->second, &policy);
    if (!policy.empty())
      bundle->Get(PolicyNamespace(domain, it->first)).Swap(&policy);
  }
}

void PolicyLoaderMac::LoadPolicyForComponent(
    const std::string& bundle_id_string,
    const Schema& schema,
    PolicyMap* policy) {
  // TODO(joaodasilva): Extensions may be registered in a ComponentMap
  // without a schema, to allow a graceful update of the Legacy Browser Support
  // extension on Windows. Remove this check once that support is removed.
  if (!schema.valid())
    return;

  base::ScopedCFTypeRef<CFStringRef> bundle_id(
      base::SysUTF8ToCFStringRef(bundle_id_string));
  preferences_->AppSynchronize(bundle_id);

  for (Schema::Iterator it = schema.GetPropertiesIterator();
       !it.IsAtEnd(); it.Advance()) {
    base::ScopedCFTypeRef<CFStringRef> pref_name(
        base::SysUTF8ToCFStringRef(it.key()));
    base::ScopedCFTypeRef<CFPropertyListRef> value(
        preferences_->CopyAppValue(pref_name, bundle_id));
    if (!value.get())
      continue;
    bool forced =
        preferences_->AppValueIsForced(pref_name, bundle_id);
    PolicyLevel level = forced ? POLICY_LEVEL_MANDATORY :
                                 POLICY_LEVEL_RECOMMENDED;
    scoped_ptr<base::Value> policy_value(CreateValueFromProperty(value));
    if (policy_value) {
      policy->Set(it.key(), level, POLICY_SCOPE_USER,
                  policy_value.release(), NULL);
    }
  }
}

void PolicyLoaderMac::OnFileUpdated(const base::FilePath& path, bool error) {
  if (!error)
    Reload(false);
}

}  // namespace policy
