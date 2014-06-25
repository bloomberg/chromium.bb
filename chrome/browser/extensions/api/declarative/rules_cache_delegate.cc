// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_cache_delegate.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/declarative/rules_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/permissions/permissions_data.h"

namespace {

// Returns the key to use for storing declarative rules in the state store.
std::string GetDeclarativeRuleStorageKey(const std::string& event_name,
                                         bool incognito) {
  if (incognito)
    return "declarative_rules.incognito." + event_name;
  else
    return "declarative_rules." + event_name;
}


}  // namespace

namespace extensions {

// RulesCacheDelegate

const char RulesCacheDelegate::kRulesStoredKey[] =
    "has_declarative_rules";

RulesCacheDelegate::RulesCacheDelegate(bool log_storage_init_delay)
    : profile_(NULL),
      log_storage_init_delay_(log_storage_init_delay),
      notified_registry_(false),
      weak_ptr_factory_(this) {
}

RulesCacheDelegate::~RulesCacheDelegate() {}

// Returns the key to use for storing whether the rules have been stored.
// static
std::string RulesCacheDelegate::GetRulesStoredKey(const std::string& event_name,
                                                  bool incognito) {
  std::string result(kRulesStoredKey);
  result += incognito ? ".incognito." : ".";
  return result + event_name;
}

// This is called from the constructor of RulesRegistry, so it is
// important that it both
// 1. calls no (in particular virtual) methods of the rules registry, and
// 2. does not create scoped_refptr holding the registry. (A short-lived
// scoped_refptr might delete the rules registry before it is constructed.)
void RulesCacheDelegate::Init(RulesRegistry* registry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // WARNING: The first use of |registry_| will bind it to the calling thread
  // so don't use this here.
  registry_ = registry->GetWeakPtr();

  profile_ = registry->profile();
  storage_key_ =
      GetDeclarativeRuleStorageKey(registry->event_name(),
                                   profile_->IsOffTheRecord());
  rules_stored_key_ = GetRulesStoredKey(registry->event_name(),
                                        profile_->IsOffTheRecord());
  rules_registry_thread_ = registry->owner_thread();

  ExtensionSystem& system = *ExtensionSystem::Get(profile_);
  StateStore* store = system.rules_store();
  if (store)
    store->RegisterKey(storage_key_);

  if (profile_->IsOffTheRecord())
    log_storage_init_delay_ = false;

  system.ready().Post(
      FROM_HERE,
      base::Bind(&RulesCacheDelegate::ReadRulesForInstalledExtensions,
                 weak_ptr_factory_.GetWeakPtr()));
  system.ready().Post(FROM_HERE,
                      base::Bind(&RulesCacheDelegate::CheckIfReady,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void RulesCacheDelegate::WriteToStorage(const std::string& extension_id,
                                     scoped_ptr<base::Value> value) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile_)
    return;

  const base::ListValue* rules = NULL;
  CHECK(value->GetAsList(&rules));
  bool rules_stored_previously = GetDeclarativeRulesStored(extension_id);
  bool store_rules = !rules->empty();
  SetDeclarativeRulesStored(extension_id, store_rules);
  if (!rules_stored_previously && !store_rules)
    return;

  StateStore* store = ExtensionSystem::Get(profile_)->rules_store();
  if (store)
    store->SetExtensionValue(extension_id, storage_key_, value.Pass());
}

void RulesCacheDelegate::CheckIfReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (notified_registry_ || !waiting_for_extensions_.empty())
    return;

  content::BrowserThread::PostTask(
      rules_registry_thread_,
      FROM_HERE,
      base::Bind(
          &RulesRegistry::MarkReady, registry_, storage_init_time_));
  notified_registry_ = true;
}

void RulesCacheDelegate::ReadRulesForInstalledExtensions() {
  ExtensionSystem& system = *ExtensionSystem::Get(profile_);
  ExtensionService* extension_service = system.extension_service();
  DCHECK(extension_service);
  // In an OTR profile, we start on top of a normal profile already, so the
  // extension service should be ready.
  DCHECK(!profile_->IsOffTheRecord() || extension_service->is_ready());
  if (extension_service->is_ready()) {
    const ExtensionSet* extensions = extension_service->extensions();
    for (ExtensionSet::const_iterator i = extensions->begin();
         i != extensions->end();
         ++i) {
      bool needs_apis_storing_rules =
          (*i)->permissions_data()->HasAPIPermission(
              APIPermission::kDeclarativeContent) ||
          (*i)->permissions_data()->HasAPIPermission(
              APIPermission::kDeclarativeWebRequest);
      bool respects_off_the_record =
          !(profile_->IsOffTheRecord()) ||
          util::IsIncognitoEnabled((*i)->id(), profile_);
      if (needs_apis_storing_rules && respects_off_the_record)
        ReadFromStorage((*i)->id());
    }
  }
}

void RulesCacheDelegate::ReadFromStorage(const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile_)
    return;

  if (log_storage_init_delay_ && storage_init_time_.is_null())
    storage_init_time_ = base::Time::Now();

  if (!GetDeclarativeRulesStored(extension_id)) {
    ExtensionSystem::Get(profile_)->ready().Post(
        FROM_HERE, base::Bind(&RulesCacheDelegate::CheckIfReady,
                              weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  StateStore* store = ExtensionSystem::Get(profile_)->rules_store();
  if (!store)
    return;
  waiting_for_extensions_.insert(extension_id);
  store->GetExtensionValue(
      extension_id,
      storage_key_,
      base::Bind(&RulesCacheDelegate::ReadFromStorageCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 extension_id));
}

void RulesCacheDelegate::ReadFromStorageCallback(
    const std::string& extension_id,
    scoped_ptr<base::Value> value) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(
      rules_registry_thread_,
      FROM_HERE,
      base::Bind(&RulesRegistry::DeserializeAndAddRules,
                 registry_,
                 extension_id,
                 base::Passed(&value)));

  waiting_for_extensions_.erase(extension_id);

  if (waiting_for_extensions_.empty())
    ExtensionSystem::Get(profile_)->ready().Post(
        FROM_HERE, base::Bind(&RulesCacheDelegate::CheckIfReady,
                              weak_ptr_factory_.GetWeakPtr()));
}

bool RulesCacheDelegate::GetDeclarativeRulesStored(
    const std::string& extension_id) const {
  CHECK(profile_);
  const ExtensionScopedPrefs* extension_prefs = ExtensionPrefs::Get(profile_);

  bool rules_stored = true;
  if (extension_prefs->ReadPrefAsBoolean(
          extension_id, rules_stored_key_, &rules_stored))
    return rules_stored;

  // Safe default -- if we don't know that the rules are not stored, we force
  // a read by returning true.
  return true;
}

void RulesCacheDelegate::SetDeclarativeRulesStored(
    const std::string& extension_id,
    bool rules_stored) {
  CHECK(profile_);
  DCHECK(ExtensionRegistry::Get(profile_)
             ->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING));

  ExtensionScopedPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  extension_prefs->UpdateExtensionPref(
      extension_id,
      rules_stored_key_,
      new base::FundamentalValue(rules_stored));
}

}  // namespace extensions
