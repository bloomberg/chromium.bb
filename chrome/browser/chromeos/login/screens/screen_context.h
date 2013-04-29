// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_CONTEXT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/string16.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler_utils.h"

namespace chromeos {

// ScreenContext is a key-value storage for values that are shared
// between C++ and JS sides. Objects of this class should be used in
// the following way:
//
// context.SetString("user", "john");
// context.SetInteger("image-index", 0);
// context.SetDouble("zoom", 1.25);
// context.GetChangesAndReset(&dictionary);
// CallJS("onContextChanged", dictionary);
//
// ScreenContext memorizes changed key-value pairs and returns them
// via GetChangesAndReset() method. After call to this method an
// internal buffer of changes will be cleared.
class ScreenContext : public base::NonThreadSafe {
 public:
  typedef std::string KeyType;
  typedef base::Value ValueType;

  ScreenContext();
  ~ScreenContext();

  bool SetBoolean(const KeyType& key, bool value);
  bool SetInteger(const KeyType& key, int value);
  bool SetDouble(const KeyType& key, double value);
  bool SetString(const KeyType& key, const std::string& value);
  bool SetString(const KeyType& key, const base::string16& value);

  bool GetBoolean(const KeyType& key);
  bool GetBoolean(const KeyType& key, bool default_value);
  int GetInteger(const KeyType& key);
  int GetInteger(const KeyType& key, int default_value);
  double GetDouble(const KeyType& key);
  double GetDouble(const KeyType& key, double default_value);
  std::string GetString(const KeyType& key);
  std::string GetString(const KeyType& key, const std::string& default_value);
  base::string16 GetString16(const KeyType& key);
  base::string16 GetString16(const KeyType& key,
                             const base::string16& default_value);

  // Returns true if context has |key|.
  bool HasKey(const KeyType& key) const;

  // Returns true if there was changes since last call to
  // GetChangesAndReset().
  bool HasChanges() const;

  // Stores all changes since the last call to the
  // GetChangesAndReset() in |diff|.  All previous contents of |diff|
  // will be thrown away.
  void GetChangesAndReset(DictionaryValue* diff);

  // Applies changes from |diff| to the context. All keys from |diff|
  // are stored in |keys|.
  void ApplyChanges(const DictionaryValue& diff,
                    std::vector<std::string>* keys);

 private:
  bool Set(const KeyType& key, Value* value);

  template<typename T>
  T Get(const KeyType& key) {
    DCHECK(CalledOnValidThread());
    const Value* value;
    bool has_key = storage_.Get(key, &value);
    DCHECK(has_key);
    T result;
    if (!ParseValue<T>(value, &result)) {
      NOTREACHED();
      return T();
    }
    return result;
  }

  template<typename T>
  T Get(const KeyType& key, const T& default_value) {
    DCHECK(CalledOnValidThread());
    if (!HasKey(key))
      return default_value;
    return Get<T>(key);
  }

  // Contains current state of <key, value> map.
  DictionaryValue storage_;

  // Contains all pending changes.
  DictionaryValue changes_;

  DISALLOW_COPY_AND_ASSIGN(ScreenContext);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_CONTEXT_H_
