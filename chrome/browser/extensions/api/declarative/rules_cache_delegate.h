// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_CACHE_DELEGATE_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_CACHE_DELEGATE_H__

#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {

class RulesRegistry;

// RulesCacheDelegate implements the part of the RulesRegistry which works on
// the UI thread. It should only be used on the UI thread. It gets created
// by the RulesRegistry, but right after that it changes owner to the
// RulesRegistryService, and is deleted by the service.
// If |log_storage_init_delay| is set, the delay caused by loading and
// registering rules on initialization will be logged with UMA.
class RulesCacheDelegate : public content::NotificationObserver {
  public:
  // |event_name| identifies the JavaScript event for which rules are
  // registered. For example, for WebRequestRulesRegistry the name is
  // "declarativeWebRequest.onRequest".
  RulesCacheDelegate(Profile* profile,
                     const std::string& event_name,
                     content::BrowserThread::ID rules_registry_thread,
                     base::WeakPtr<RulesRegistry> registry,
                     bool log_storage_init_delay);

  virtual ~RulesCacheDelegate();

  // Returns a key for the state store. The associated preference is a boolean
  // indicating whether there are some declarative rules stored in the rule
  // store.
  static std::string GetRulesStoredKey(const std::string& event_name,
                                       bool incognito);

  // Initialize the storage functionality.
  void Init();

  void WriteToStorage(const std::string& extension_id,
                      scoped_ptr<base::Value> value);

  base::WeakPtr<RulesCacheDelegate> GetWeakPtr() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    return weak_ptr_factory_.GetWeakPtr();
  }

  private:
  FRIEND_TEST_ALL_PREFIXES(RulesRegistryWithCacheTest,
                           DeclarativeRulesStored);
  FRIEND_TEST_ALL_PREFIXES(RulesRegistryWithCacheTest,
                           RulesStoredFlagMultipleRegistries);

  static const char kRulesStoredKey[];

  // NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Check if we are done reading all data from storage on startup, and notify
  // the RulesRegistry on its thread if so. The notification is delivered
  // exactly once.
  void CheckIfReady();

  // Schedules retrieving rules for already loaded extensions where
  // appropriate.
  void ReadRulesForInstalledExtensions();

  // Read/write a list of rules serialized to Values.
  void ReadFromStorage(const std::string& extension_id);
  void ReadFromStorageCallback(const std::string& extension_id,
                               scoped_ptr<base::Value> value);

  // Check the preferences whether the extension with |extension_id| has some
  // rules stored on disk. If this information is not in the preferences, true
  // is returned as a safe default value.
  bool GetDeclarativeRulesStored(const std::string& extension_id) const;
  // Modify the preference to |rules_stored|.
  void SetDeclarativeRulesStored(const std::string& extension_id,
                                 bool rules_stored);

  content::NotificationRegistrar registrar_;

  Profile* profile_;

  // The key under which rules are stored.
  const std::string storage_key_;

  // The key under which we store whether the rules have been stored.
  const std::string rules_stored_key_;

  // A set of extension IDs that have rules we are reading from storage.
  std::set<std::string> waiting_for_extensions_;

  // We measure the time spent on loading rules on init. The result is logged
  // with UMA once per each RulesCacheDelegate instance, unless in Incognito.
  base::Time storage_init_time_;
  bool log_storage_init_delay_;

  // Weak pointer to post tasks to the owning rules registry.
  const base::WeakPtr<RulesRegistry> registry_;

  // The thread |registry_| lives on.
  const content::BrowserThread::ID rules_registry_thread_;

  // We notified the RulesRegistry that the rules are loaded.
  bool notified_registry_;

  // Use this factory to generate weak pointers bound to the UI thread.
  base::WeakPtrFactory<RulesCacheDelegate> weak_ptr_factory_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_CACHE_DELEGATE_H__
