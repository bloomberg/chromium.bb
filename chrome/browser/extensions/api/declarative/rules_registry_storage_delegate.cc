// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry_storage_delegate.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
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

namespace extensions {

namespace {

scoped_ptr<base::Value> RulesToValue(
    const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) {
  scoped_ptr<base::ListValue> list(new base::ListValue());
  for (size_t i = 0; i < rules.size(); ++i)
    list->Append(rules[i]->ToValue().release());
  return list.PassAs<base::Value>();
}

std::vector<linked_ptr<RulesRegistry::Rule> > RulesFromValue(
    base::Value* value) {
  std::vector<linked_ptr<RulesRegistry::Rule> > rules;

  base::ListValue* list = NULL;
  if (!value || !value->GetAsList(&list))
    return rules;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* dict = NULL;
    if (!list->GetDictionary(i, &dict))
      continue;
    linked_ptr<RulesRegistry::Rule> rule(new RulesRegistry::Rule());
    if (RulesRegistry::Rule::Populate(*dict, rule.get()))
      rules.push_back(rule);
  }

  return rules;
}

}  // namespace

// This class coordinates information between the UI and RulesRegistry threads.
// It may outlive the RulesRegistry, which owns the delegate. Methods/variables
// should be used on the UI thread unless otherwise noted.
class RulesRegistryStorageDelegate::Inner
    : public content::NotificationObserver,
      public base::RefCountedThreadSafe<Inner> {
 public:
  Inner(Profile* profile,
        RulesRegistryWithCache* rules_registry,
        const std::string& storage_key);

 private:
  friend class base::RefCountedThreadSafe<Inner>;
  friend class RulesRegistryStorageDelegate;

  virtual ~Inner();

  // Initialization of the storage delegate if it is used in the context of
  // an incognito profile.
  void InitForOTRProfile();

  // NotificationObserver
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

  // Read/write a list of rules serialized to Values.
  void ReadFromStorage(const std::string& extension_id);
  void ReadFromStorageCallback(const std::string& extension_id,
                       scoped_ptr<base::Value> value);
  void WriteToStorage(const std::string& extension_id,
                      scoped_ptr<base::Value> value);

  // Check if we are done reading all data from storage on startup, and notify
  // the RulesRegistry on its thread if so. The notification is delivered
  // exactly once.
  void CheckIfReady();

  // Deserialize the rules from the given Value object and add them to the
  // RulesRegistry.
  void ReadFromStorageOnRegistryThread(const std::string& extension_id,
                                       scoped_ptr<base::Value> value);

  // Notify the RulesRegistry that we are now ready.
  void NotifyReadyOnRegistryThread();

  scoped_ptr<content::NotificationRegistrar> registrar_;
  Profile* profile_;

  // The key under which rules are stored.
  const std::string storage_key_;

  // A set of extension IDs that have rules we are reading from storage.
  std::set<std::string> waiting_for_extensions_;

  // The thread that our RulesRegistry lives on.
  content::BrowserThread::ID rules_registry_thread_;

  // The following are only accessible on rules_registry_thread_.

  // The RulesRegistry whose delegate we are.
  RulesRegistryWithCache* rules_registry_;

  // True when we have finished reading from storage for all extensions that
  // are loaded on startup.
  bool ready_;

  // We measure the time spent on loading rules on init. The result is logged
  // with UMA once per the delegate instance, unless in Incognito.
  base::Time storage_init_time_;
  bool log_storage_init_delay_;
  // TODO(vabr): Could |ready_| be used instead of |log_storage_init_delay_|?
  // http://crbug.com/176926
};

RulesRegistryStorageDelegate::RulesRegistryStorageDelegate() {
}

RulesRegistryStorageDelegate::~RulesRegistryStorageDelegate() {
  // RulesRegistry owns us, which means it has been deleted.
  inner_->rules_registry_ = NULL;
}

void RulesRegistryStorageDelegate::InitOnUIThread(
    Profile* profile,
    RulesRegistryWithCache* rules_registry,
    const std::string& storage_key) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  extensions::StateStore* store = ExtensionSystem::Get(profile)->rules_store();
  if (store)
    store->RegisterKey(storage_key);
  inner_ = new Inner(profile, rules_registry, storage_key);
}

void RulesRegistryStorageDelegate::CleanupOnUIThread() {
  // The registrar must be deleted on the UI thread.
  inner_->registrar_.reset();
  inner_->profile_ = NULL;  // no longer safe to use.
}

bool RulesRegistryStorageDelegate::IsReady() {
  DCHECK(content::BrowserThread::CurrentlyOn(inner_->rules_registry_thread_));
  return inner_->ready_;
}

void RulesRegistryStorageDelegate::OnRulesChanged(
    RulesRegistryWithCache* rules_registry,
    const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(inner_->rules_registry_thread_));
  std::vector<linked_ptr<RulesRegistry::Rule> > new_rules;
  std::string error = rules_registry->GetAllRules(extension_id, &new_rules);
  DCHECK_EQ("", error);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&Inner::WriteToStorage, inner_.get(), extension_id,
                 base::Passed(RulesToValue(new_rules))));
}

RulesRegistryStorageDelegate::Inner::Inner(
    Profile* profile,
    RulesRegistryWithCache* rules_registry,
    const std::string& storage_key)
    : registrar_(new content::NotificationRegistrar()),
      profile_(profile),
      storage_key_(storage_key),
      rules_registry_thread_(rules_registry->GetOwnerThread()),
      rules_registry_(rules_registry),
      ready_(false),
      log_storage_init_delay_(true) {
  if (!profile_->IsOffTheRecord()) {
    registrar_->Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                    content::Source<Profile>(profile));
    registrar_->Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                    content::Source<Profile>(profile));
  } else {
    log_storage_init_delay_ = false;
    registrar_->Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                    content::Source<Profile>(profile->GetOriginalProfile()));
    InitForOTRProfile();
  }
}

RulesRegistryStorageDelegate::Inner::~Inner() {
  DCHECK(!registrar_.get());
}

void RulesRegistryStorageDelegate::Inner::InitForOTRProfile() {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const ExtensionSet* extensions = extension_service->extensions();
  for (ExtensionSet::const_iterator i = extensions->begin();
       i != extensions->end(); ++i) {
    if (((*i)->HasAPIPermission(APIPermission::kDeclarativeContent) ||
         (*i)->HasAPIPermission(APIPermission::kDeclarativeWebRequest)) &&
        extension_service->IsIncognitoEnabled((*i)->id()))
      ReadFromStorage((*i)->id());
  }
  ready_ = true;
}

void RulesRegistryStorageDelegate::Inner::Observe(
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
    CheckIfReady();
  }
}

void RulesRegistryStorageDelegate::Inner::ReadFromStorage(
    const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!profile_)
    return;

  if (storage_init_time_.is_null())
    storage_init_time_ = base::Time::Now();

  extensions::StateStore* store = ExtensionSystem::Get(profile_)->rules_store();
  if (store) {
    waiting_for_extensions_.insert(extension_id);
    store->GetExtensionValue(extension_id, storage_key_,
        base::Bind(&Inner::ReadFromStorageCallback, this, extension_id));
  }

  // TODO(mpcomplete): Migration code. Remove when declarativeWebRequest goes
  // to stable.
  // http://crbug.com/166474
  store = ExtensionSystem::Get(profile_)->state_store();
  if (store) {
    waiting_for_extensions_.insert(extension_id);
    store->GetExtensionValue(extension_id, storage_key_,
        base::Bind(&Inner::ReadFromStorageCallback, this, extension_id));
    store->RemoveExtensionValue(extension_id, storage_key_);
  }
}

void RulesRegistryStorageDelegate::Inner::ReadFromStorageCallback(
    const std::string& extension_id, scoped_ptr<base::Value> value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(
      rules_registry_thread_, FROM_HERE,
      base::Bind(&Inner::ReadFromStorageOnRegistryThread, this, extension_id,
                 base::Passed(&value)));

  waiting_for_extensions_.erase(extension_id);
  CheckIfReady();
  if (log_storage_init_delay_ && waiting_for_extensions_.empty()) {
    UMA_HISTOGRAM_TIMES("Extensions.DeclarativeRulesStorageInitialization",
                        base::Time::Now() - storage_init_time_);
    log_storage_init_delay_ = false;
  }
}

void RulesRegistryStorageDelegate::Inner::WriteToStorage(
    const std::string& extension_id, scoped_ptr<base::Value> value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!profile_)
    return;

  StateStore* store = ExtensionSystem::Get(profile_)->rules_store();
  if (store)
    store->SetExtensionValue(extension_id, storage_key_, value.Pass());
}

void RulesRegistryStorageDelegate::Inner::CheckIfReady() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!waiting_for_extensions_.empty())
    return;

  content::BrowserThread::PostTask(
      rules_registry_thread_, FROM_HERE,
      base::Bind(&Inner::NotifyReadyOnRegistryThread, this));
}

void RulesRegistryStorageDelegate::Inner::ReadFromStorageOnRegistryThread(
    const std::string& extension_id, scoped_ptr<base::Value> value) {
  DCHECK(content::BrowserThread::CurrentlyOn(rules_registry_thread_));
  if (!rules_registry_)
    return;  // registry went away

  rules_registry_->AddRules(extension_id, RulesFromValue(value.get()));
}

void RulesRegistryStorageDelegate::Inner::NotifyReadyOnRegistryThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(rules_registry_thread_));
  if (ready_)
    return;  // we've already notified our readiness

  ready_ = true;
  rules_registry_->OnReady();
}

}  // namespace extensions
