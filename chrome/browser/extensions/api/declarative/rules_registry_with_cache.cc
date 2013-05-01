// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry_with_cache.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace {

const char kSuccess[] = "";
const char kDuplicateRuleId[] = "Duplicate rule ID: %s";

scoped_ptr<base::Value> RulesToValue(
    const std::vector<linked_ptr<extensions::RulesRegistry::Rule> >& rules) {
  scoped_ptr<base::ListValue> list(new base::ListValue());
  for (size_t i = 0; i < rules.size(); ++i)
    list->Append(rules[i]->ToValue().release());
  return list.PassAs<base::Value>();
}

std::vector<linked_ptr<extensions::RulesRegistry::Rule> > RulesFromValue(
    const base::Value* value) {
  std::vector<linked_ptr<extensions::RulesRegistry::Rule> > rules;

  const base::ListValue* list = NULL;
  if (!value || !value->GetAsList(&list))
    return rules;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* dict = NULL;
    if (!list->GetDictionary(i, &dict))
      continue;
    linked_ptr<extensions::RulesRegistry::Rule> rule(
        new extensions::RulesRegistry::Rule());
    if (extensions::RulesRegistry::Rule::Populate(*dict, rule.get()))
      rules.push_back(rule);
  }

  return rules;
}

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

// RulesRegistryWithCache

RulesRegistryWithCache::RulesRegistryWithCache(
    Profile* profile,
    const char* event_name,
    content::BrowserThread::ID owner_thread,
    bool log_storage_init_delay,
    scoped_ptr<RuleStorageOnUI>* ui_part)
    : RulesRegistry(owner_thread, event_name),
      ready_(false),
      weak_ptr_factory_((profile) ? this : NULL),
      storage_on_ui_((profile
                          ? (new RuleStorageOnUI(profile,
                                                 GetDeclarativeRuleStorageKey(
                                                     event_name,
                                                     profile->IsOffTheRecord()),
                                                 owner_thread,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 log_storage_init_delay))
                                ->GetWeakPtr()
                          : base::WeakPtr<RuleStorageOnUI>())) {
  if (!profile) {
    CHECK(!ui_part);
    return;
  }

  ui_part->reset(storage_on_ui_.get());

  // After construction, |this| continues to live only on |owner_thread|.
  weak_ptr_factory_.DetachFromThread();

  storage_on_ui_->Init();
}

void RulesRegistryWithCache::AddReadyCallback(const base::Closure& callback) {
  ready_callbacks_.push_back(callback);
}

std::string RulesRegistryWithCache::AddRules(
    const std::string& extension_id,
    const std::vector<linked_ptr<Rule> >& rules) {
  DCHECK(content::BrowserThread::CurrentlyOn(owner_thread()));

  // Verify that all rule IDs are new.
  for (std::vector<linked_ptr<Rule> >::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    const RuleId& rule_id = *((*i)->id);
    RulesDictionaryKey key(extension_id, rule_id);
    if (rules_.find(key) != rules_.end())
      return base::StringPrintf(kDuplicateRuleId, rule_id.c_str());
  }

  std::string error = AddRulesImpl(extension_id, rules);

  if (!error.empty())
    return error;

  // Commit all rules into |rules_| on success.
  for (std::vector<linked_ptr<Rule> >::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    const RuleId& rule_id = *((*i)->id);
    RulesDictionaryKey key(extension_id, rule_id);
    rules_[key] = *i;
  }

  ProcessChangedRules(extension_id);
  return kSuccess;
}

std::string RulesRegistryWithCache::RemoveRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  DCHECK(content::BrowserThread::CurrentlyOn(owner_thread()));

  std::string error = RemoveRulesImpl(extension_id, rule_identifiers);

  if (!error.empty())
    return error;

  // Commit removal of rules from |rules_| on success.
  for (std::vector<std::string>::const_iterator i =
      rule_identifiers.begin(); i != rule_identifiers.end(); ++i) {
    RulesDictionaryKey lookup_key(extension_id, *i);
    rules_.erase(lookup_key);
  }

  ProcessChangedRules(extension_id);
  return kSuccess;
}

std::string RulesRegistryWithCache::RemoveAllRules(
    const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(owner_thread()));

  std::string error = RemoveAllRulesImpl(extension_id);

  if (!error.empty())
    return error;

  // Commit removal of rules from |rules_| on success.
  for (RulesDictionary::const_iterator i = rules_.begin();
      i != rules_.end();) {
    const RulesDictionaryKey& key = i->first;
    ++i;
    if (key.first == extension_id)
      rules_.erase(key);
  }

  ProcessChangedRules(extension_id);
  return kSuccess;
}

std::string RulesRegistryWithCache::GetRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers,
    std::vector<linked_ptr<RulesRegistry::Rule> >* out) {
  DCHECK(content::BrowserThread::CurrentlyOn(owner_thread()));

  for (std::vector<std::string>::const_iterator i = rule_identifiers.begin();
      i != rule_identifiers.end(); ++i) {
    RulesDictionaryKey lookup_key(extension_id, *i);
    RulesDictionary::iterator entry = rules_.find(lookup_key);
    if (entry != rules_.end())
      out->push_back(entry->second);
  }
  return kSuccess;
}

std::string RulesRegistryWithCache::GetAllRules(
    const std::string& extension_id,
    std::vector<linked_ptr<RulesRegistry::Rule> >* out) {
  DCHECK(content::BrowserThread::CurrentlyOn(owner_thread()));

  for (RulesDictionary::const_iterator i = rules_.begin();
      i != rules_.end(); ++i) {
    const RulesDictionaryKey& key = i->first;
    if (key.first == extension_id)
      out->push_back(i->second);
  }
  return kSuccess;
}

void RulesRegistryWithCache::OnExtensionUnloaded(
    const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(owner_thread()));
  std::string error = RemoveAllRules(extension_id);
  if (!error.empty())
    LOG(ERROR) << error;
}

RulesRegistryWithCache::~RulesRegistryWithCache() {
}

void RulesRegistryWithCache::OnReady(base::Time storage_init_time) {
  DCHECK(content::BrowserThread::CurrentlyOn(owner_thread()));
  DCHECK(!ready_);  // Avoid thread hopping, only call this once.

  ready_ = true;

  if (!storage_init_time.is_null()) {
    UMA_HISTOGRAM_TIMES("Extensions.DeclarativeRulesStorageInitialization",
                        base::Time::Now() - storage_init_time);
  }

  for (size_t i = 0; i < ready_callbacks_.size(); ++i)
    ready_callbacks_[i].Run();
  ready_callbacks_.clear();
}

void RulesRegistryWithCache::DeserializeAndAddRules(
    const std::string& extension_id,
    scoped_ptr<base::Value> rules) {
  DCHECK(content::BrowserThread::CurrentlyOn(owner_thread()));

  AddRules(extension_id, RulesFromValue(rules.get()));
}

void RulesRegistryWithCache::ProcessChangedRules(
    const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(owner_thread()));

  std::vector<linked_ptr<RulesRegistry::Rule> > new_rules;
  std::string error = GetAllRules(extension_id, &new_rules);
  DCHECK_EQ(std::string(), error);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RuleStorageOnUI::WriteToStorage,
                 storage_on_ui_,
                 extension_id,
                 base::Passed(RulesToValue(new_rules))));
}

// RulesRegistryWithCache::RuleStorageOnUI

RulesRegistryWithCache::RuleStorageOnUI::RuleStorageOnUI(
    Profile* profile,
    const std::string& storage_key,
    content::BrowserThread::ID rules_registry_thread,
    base::WeakPtr<RulesRegistryWithCache> registry,
    bool log_storage_init_delay)
    : profile_(profile),
      storage_key_(storage_key),
      log_storage_init_delay_(log_storage_init_delay),
      registry_(registry),
      rules_registry_thread_(rules_registry_thread),
      ready_state_(NOT_READY),
      weak_ptr_factory_(this) {}

RulesRegistryWithCache::RuleStorageOnUI::~RuleStorageOnUI() {}

// This is called from the constructor of RulesRegistryWithCache, so it is
// important that it both
// 1. calls no (in particular virtual) methods of the rules registry, and
// 2. does not create scoped_refptr holding the registry. (A short-lived
// scoped_refptr might delete the rules registry before it is constructed.)
void RulesRegistryWithCache::RuleStorageOnUI::Init() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  extensions::StateStore* store = ExtensionSystem::Get(profile_)->rules_store();
  if (store)
    store->RegisterKey(storage_key_);

  if (!profile_->IsOffTheRecord()) {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                    content::Source<Profile>(profile_));
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                    content::Source<Profile>(profile_));
  } else {
    log_storage_init_delay_ = false;
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                   content::Source<Profile>(profile_->GetOriginalProfile()));
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    DCHECK(extension_service->is_ready());
    const ExtensionSet* extensions = extension_service->extensions();
    for (ExtensionSet::const_iterator i = extensions->begin();
         i != extensions->end();
         ++i) {
      if (((*i)->HasAPIPermission(APIPermission::kDeclarativeContent) ||
           (*i)->HasAPIPermission(APIPermission::kDeclarativeWebRequest)) &&
          extension_service->IsIncognitoEnabled((*i)->id()))
        ReadFromStorage((*i)->id());
    }
    // The extensions are already ready for the original profile.
    ready_state_ = EXTENSIONS_READY;
    // Init is called from the registry's constructor, and because CheckIfReady
    // can post tasks for the not-yet-initialized registry to run, we need to
    // postpone checking for being ready (=all rules loaded), on the registry's
    // thread.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&RuleStorageOnUI::CheckIfReady, GetWeakPtr()));
  }
}

void RulesRegistryWithCache::RuleStorageOnUI::WriteToStorage(
    const std::string& extension_id,
    scoped_ptr<base::Value> value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!profile_)
    return;

  StateStore* store = ExtensionSystem::Get(profile_)->rules_store();
  if (store)
    store->SetExtensionValue(extension_id, storage_key_, value.Pass());
}

void RulesRegistryWithCache::RuleStorageOnUI::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
    const extensions::Extension* extension =
        content::Details<const extensions::Extension>(details).ptr();
    // TODO(mpcomplete): This API check should generalize to any use of
    // declarative rules, not just webRequest.
    if (extension->HasAPIPermission(APIPermission::kDeclarativeContent) ||
        extension->HasAPIPermission(APIPermission::kDeclarativeWebRequest)) {
      ExtensionInfoMap* extension_info_map =
          ExtensionSystem::Get(profile_)->info_map();
      if (profile_->IsOffTheRecord() &&
          !extension_info_map->IsIncognitoEnabled(extension->id())) {
        // Ignore this extension.
      } else {
        ReadFromStorage(extension->id());
      }
    }
  } else if (type == chrome::NOTIFICATION_EXTENSIONS_READY) {
    CHECK_EQ(NOT_READY, ready_state_);
    ready_state_ = EXTENSIONS_READY;
    CheckIfReady();
  }
}

void RulesRegistryWithCache::RuleStorageOnUI::CheckIfReady() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (ready_state_ != EXTENSIONS_READY || !waiting_for_extensions_.empty())
    return;

  content::BrowserThread::PostTask(
      rules_registry_thread_,
      FROM_HERE,
      base::Bind(
          &RulesRegistryWithCache::OnReady, registry_, storage_init_time_));
  ready_state_ = NOTIFIED_READY;
}

void RulesRegistryWithCache::RuleStorageOnUI::ReadFromStorage(
    const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!profile_)
    return;

  extensions::StateStore* store = ExtensionSystem::Get(profile_)->rules_store();
  if (!store)
    return;
  waiting_for_extensions_.insert(extension_id);
  if (log_storage_init_delay_ && storage_init_time_.is_null())
    storage_init_time_ = base::Time::Now();
  store->GetExtensionValue(extension_id,
                           storage_key_,
                           base::Bind(&RuleStorageOnUI::ReadFromStorageCallback,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      extension_id));
}

void RulesRegistryWithCache::RuleStorageOnUI::ReadFromStorageCallback(
    const std::string& extension_id,
    scoped_ptr<base::Value> value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(
      rules_registry_thread_,
      FROM_HERE,
      base::Bind(&RulesRegistryWithCache::DeserializeAndAddRules,
                 registry_,
                 extension_id,
                 base::Passed(&value)));

  waiting_for_extensions_.erase(extension_id);

  CheckIfReady();
}

}  // namespace extensions
