// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_pref_store.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"

ExtensionPrefStore::ExtensionPrefStore(Profile* profile,
                                       PrefNotifier::PrefStoreType type)
    : prefs_(new DictionaryValue()),
      profile_(profile),
      type_(type) {
  RegisterObservers();
}

ExtensionPrefStore::~ExtensionPrefStore() {
  STLDeleteElements(&extension_stack_);
  notification_registrar_.RemoveAll();
}

void ExtensionPrefStore::InstallExtensionPref(const Extension* extension,
                                              const char* new_pref_path,
                                              Value* new_pref_value) {
  ExtensionStack::iterator i;
  for (i = extension_stack_.begin(); i != extension_stack_.end(); ++i) {
    if ((*i)->extension == extension)
      break;
  }

  // If this extension is not already in the stack, add it. Otherwise, update
  // or add the value of this preference, but don't change the extension's
  // position in the stack. We store the extension even if this preference
  // isn't registered with our PrefService, so that the ordering of extensions
  // is consistent among all local-state and user ExtensionPrefStores.
  PrefService* pref_service = GetPrefService();
  // The pref_service may be NULL in unit testing.
  bool is_registered_pref = (pref_service == NULL ||
      pref_service->FindPreference(new_pref_path) != NULL);
  PrefValueMap* pref_values;
  if (i == extension_stack_.end()) {
    pref_values = new PrefValueMap();
    if (is_registered_pref)
      (*pref_values)[new_pref_path] = new_pref_value;

    ExtensionPrefs* extension_prefs = new ExtensionPrefs(extension,
                                                         pref_values);
    extension_stack_.push_front(extension_prefs);
  } else if (is_registered_pref) {
    pref_values = (*i)->pref_values;
    delete (*pref_values)[new_pref_path];
    (*pref_values)[new_pref_path] = new_pref_value;
  }

  // Apply the preference to our local |prefs_| store.
  UpdateOnePref(new_pref_path);
}

void ExtensionPrefStore::UninstallExtension(const Extension* extension) {
  // Remove this extension from the stack.
  for (ExtensionStack::iterator i = extension_stack_.begin();
      i != extension_stack_.end(); ++i) {
    if ((*i)->extension == extension) {
      scoped_ptr<ExtensionPrefs> to_be_deleted(*i);
      extension_stack_.erase(i);
      UpdatePrefs(to_be_deleted->pref_values);
      return;
    }
  }
}

void ExtensionPrefStore::GetExtensionIDs(std::vector<std::string>* result) {
  for (ExtensionStack::iterator i = extension_stack_.begin();
      i != extension_stack_.end(); ++i) {
    (*result).push_back((*i)->extension->id());
  }
}

// This could be sped up by keeping track of which extension currently controls
// a given preference, among other optimizations. But probably fewer than 10
// installed extensions will be trying to control any preferences, so stick
// with this simpler algorithm until it causes a problem.
void ExtensionPrefStore::UpdateOnePref(const char* path) {
  PrefService* pref_service = GetPrefService();

  // There are at least two PrefServices, one for local state and one for
  // user prefs. (See browser_main.cc.) Different preferences are registered
  // in each; if this one doesn't have the desired pref registered, we ignore
  // it and let the other one handle it.
  // The pref_service may be NULL in unit testing.
  if (pref_service && !pref_service->FindPreference(path))
      return;

  // Save the old value before removing it from the local cache.
  Value* my_old_value_ptr = NULL;
  prefs_->Get(path, &my_old_value_ptr);
  scoped_ptr<Value> my_old_value;
  if (my_old_value_ptr)
    my_old_value.reset(my_old_value_ptr->DeepCopy());

  // DictionaryValue::Set complains if a key is overwritten with the same
  // value, so remove it first.
  prefs_->Remove(path, NULL);

  // Find the first extension that wants to set this pref and use its value.
  Value* my_new_value = NULL;
  for (ExtensionStack::iterator ext_iter = extension_stack_.begin();
        ext_iter != extension_stack_.end(); ++ext_iter) {
    PrefValueMap::iterator value_iter = (*ext_iter)->pref_values->find(path);
    if (value_iter != (*ext_iter)->pref_values->end()) {
      prefs_->Set(path, (*value_iter).second->DeepCopy());
      my_new_value = (*value_iter).second;
      break;
    }
  }

  if (pref_service) {
    bool value_changed = true;
    if (!my_old_value.get() && !my_new_value) {
      value_changed = false;
    } else if (my_old_value.get() &&
               my_new_value &&
               my_old_value->Equals(my_new_value)) {
      value_changed = false;
    }

    if (value_changed)
      pref_service->pref_notifier()->OnPreferenceSet(path, type_);
  }
}

void ExtensionPrefStore::UpdatePrefs(const PrefValueMap* pref_values) {
  if (!pref_values)
    return;

  for (PrefValueMap::const_iterator i = pref_values->begin();
        i != pref_values->end(); ++i) {
    UpdateOnePref(i->first);
  }
}

PrefService* ExtensionPrefStore::GetPrefService() {
  if (profile_)
    return profile_->GetPrefs();
  return g_browser_process->local_state();
}

void ExtensionPrefStore::RegisterObservers() {
  notification_registrar_.Add(this,
                              NotificationType::EXTENSION_PREF_CHANGED,
                              NotificationService::AllSources());

  notification_registrar_.Add(this,
                              NotificationType::EXTENSION_UNLOADED,
                              NotificationService::AllSources());
}

void ExtensionPrefStore::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_PREF_CHANGED: {
      Profile* extension_profile = Source<Profile>(source).ptr();
      // The ExtensionPrefStore for the local state watches all profiles.
      if (!profile_ || profile_ == extension_profile) {
        ExtensionPrefStore::ExtensionPrefDetails* data =
            Details<ExtensionPrefStore::ExtensionPrefDetails>(details).ptr();
        InstallExtensionPref(data->first, data->second.first,
            data->second.second);
      }
      break;
    }
    case NotificationType::EXTENSION_UNLOADED: {
      Profile* extension_profile = Source<Profile>(source).ptr();
      const Extension* extension = Details<const Extension>(details).ptr();
      // The ExtensionPrefStore for the local state watches all profiles.
      if (profile_ == NULL || profile_ == extension_profile)
        UninstallExtension(extension);
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}

ExtensionPrefStore::ExtensionPrefs::ExtensionPrefs(const Extension* extension,
    PrefValueMap* values) : extension(extension), pref_values(values) {}

ExtensionPrefStore::ExtensionPrefs::~ExtensionPrefs() {
  STLDeleteValues(pref_values);
  delete pref_values;
}
