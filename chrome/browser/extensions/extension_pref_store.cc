// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_pref_store.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/values.h"
#include "chrome/browser/pref_service.h"

ExtensionPrefStore::ExtensionPrefStore(PrefService* pref_service)
    : pref_service_(pref_service),
      prefs_(new DictionaryValue()) {
}

ExtensionPrefStore::~ExtensionPrefStore() {
  STLDeleteElements(&extension_stack_);
}

// This could be sped up by keeping track of which extension currently controls
// a given preference, among other optimizations. But we estimate that fewer
// than 10 installed extensions will be trying to control any preferences, so
// stick with this simpler algorithm until it causes a problem.
void ExtensionPrefStore::UpdateOnePref(const wchar_t* path) {
  // Query the PrefService to find the current value for this pref.
  // pref_service_ might be null in unit tests.
  scoped_ptr<Value> old_value;
  if (pref_service_) {
    old_value.reset(
        pref_service_->FindPreference(path)->GetValue()->DeepCopy());
  }

  // DictionaryValue::Set complains if a key is overwritten with the same
  // value, so delete it first.
  prefs_->Remove(path, NULL);

  // Find the first extension that wants to set this pref and use its value.
  for (ExtensionStack::iterator ext_iter = extension_stack_.begin();
        ext_iter != extension_stack_.end(); ++ext_iter) {
    PrefValueMap::iterator value_iter = (*ext_iter)->pref_values->find(path);
    if (value_iter != (*ext_iter)->pref_values->end()) {
      prefs_->Set(path, (*value_iter).second->DeepCopy());
      break;
    }
  }
  if (pref_service_)
    pref_service_->FireObserversIfChanged(path, old_value.get());
}

void ExtensionPrefStore::UpdatePrefs(const PrefValueMap* pref_values) {
  for (PrefValueMap::const_iterator i = pref_values->begin();
        i != pref_values->end(); ++i) {
    UpdateOnePref(i->first);
  }
}

void ExtensionPrefStore::InstallExtensionPref(std::string extension_id,
                                              const wchar_t* pref_path,
                                              Value* pref_value) {
  ExtensionStack::iterator i;
  for (i = extension_stack_.begin(); i != extension_stack_.end(); ++i) {
    if ((*i)->extension_id == extension_id)
      break;
  }

  // If this extension is already in the stack, update or add the value of this
  // preference, but don't change the extension's position in the stack.
  // Otherwise, push the new extension onto the stack.
  PrefValueMap* pref_values;
  if (i != extension_stack_.end()) {
    pref_values = (*i)->pref_values;
    (*pref_values)[pref_path] = pref_value;
  } else {
    pref_values = new PrefValueMap();
    (*pref_values)[pref_path] = pref_value;

    ExtensionPrefs* extension_prefs = new ExtensionPrefs();
    extension_prefs->extension_id = extension_id;
    extension_prefs->pref_values = pref_values;
    extension_stack_.push_front(extension_prefs);
  }

  // Look for an old value with the same type as the one we're modifying.
  UpdateOnePref(pref_path);
}

void ExtensionPrefStore::UninstallExtension(std::string extension_id) {
  // Remove this extension from the stack.
  scoped_ptr<PrefValueMap> pref_values;
  for (ExtensionStack::iterator i = extension_stack_.begin();
      i != extension_stack_.end(); ++i) {
    if ((*i)->extension_id == extension_id) {
      pref_values.reset((*i)->pref_values);
      delete *i;
      extension_stack_.erase(i);
      break;
    }
  }
  if (!pref_values.get())
    return;

  UpdatePrefs(pref_values.get());
}

void ExtensionPrefStore::GetExtensionIDs(std::vector<std::string>* result) {
  for (ExtensionStack::iterator i = extension_stack_.begin();
      i != extension_stack_.end(); ++i) {
    (*result).push_back((*i)->extension_id);
  }
}
