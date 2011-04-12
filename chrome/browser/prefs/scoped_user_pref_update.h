// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class that assists preferences in firing notifications when lists
// or dictionaries are changed.

#ifndef CHROME_BROWSER_PREFS_SCOPED_USER_PREF_UPDATE_H_
#define CHROME_BROWSER_PREFS_SCOPED_USER_PREF_UPDATE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"

class DictionaryValue;
class ListValue;
class PrefService;

namespace subtle {

// Base class for ScopedUserPrefUpdateTemplate that contains the parts
// that do not depend on ScopedUserPrefUpdateTemplate's template parameter.
//
// We need this base class mostly for making it a friend of PrefService
// and getting access to PrefService::GetMutableUserPref and
// PrefService::ReportUserPrefChanged.
class ScopedUserPrefUpdateBase : public base::NonThreadSafe {
 protected:
  ScopedUserPrefUpdateBase(PrefService* service, const char* path);

  // Calls Notify().
  virtual ~ScopedUserPrefUpdateBase();

  // Sets |value_| to |service_|->GetMutableUserPref and returns it.
  Value* Get(Value::ValueType type);

 private:
  // If |value_| is not null, triggers a notification of PrefObservers and
  // resets |value_|.
  virtual void Notify();

  // Weak pointer.
  PrefService* service_;
  // Path of the preference being updated.
  std::string path_;
  // Cache of value from user pref store (set between Get() and Notify() calls).
  Value* value_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUserPrefUpdateBase);
};

}  // namespace subtle

// Class to support modifications to DictionaryValues and ListValues while
// guaranteeing that PrefObservers are notified of changed values.
//
// This class may only be used on the UI thread as it requires access to the
// PrefService.
template <typename T, Value::ValueType type_enum_value>
class ScopedUserPrefUpdate : public subtle::ScopedUserPrefUpdateBase {
 public:
  ScopedUserPrefUpdate(PrefService* service, const char* path)
      : ScopedUserPrefUpdateBase(service, path) {}

  // Triggers an update notification if Get() was called.
  virtual ~ScopedUserPrefUpdate() {}

  // Returns a mutable |T| instance that
  // - is already in the user pref store, or
  // - is (silently) created and written to the user pref store if none existed
  //   before.
  //
  // Calling Get() implies that an update notification is necessary at
  // destruction time.
  //
  // The ownership of the return value remains with the user pref store.
  virtual T* Get() {
    return static_cast<T*>(
        subtle::ScopedUserPrefUpdateBase::Get(type_enum_value));
  }

  T& operator*() {
    return *Get();
  }

  T* operator->() {
    return Get();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedUserPrefUpdate);
};

typedef ScopedUserPrefUpdate<DictionaryValue, Value::TYPE_DICTIONARY>
    DictionaryPrefUpdate;
typedef ScopedUserPrefUpdate<ListValue, Value::TYPE_LIST> ListPrefUpdate;

#endif  // CHROME_BROWSER_PREFS_SCOPED_USER_PREF_UPDATE_H_
