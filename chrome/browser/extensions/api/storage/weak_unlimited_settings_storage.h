// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_WEAK_UNLIMITED_SETTINGS_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_WEAK_UNLIMITED_SETTINGS_STORAGE_H_

#include "base/compiler_specific.h"
#include "chrome/browser/value_store/value_store.h"

namespace extensions {

// A ValueStore decorator which makes calls through |Set| ignore quota.
// "Weak" because ownership of the delegate isn't taken; this is designed to be
// temporarily attached to storage areas.
class WeakUnlimitedSettingsStorage : public ValueStore {
 public:
  // Ownership of |delegate| NOT taken.
  explicit WeakUnlimitedSettingsStorage(ValueStore* delegate);

  virtual ~WeakUnlimitedSettingsStorage();

  // ValueStore implementation.
  virtual size_t GetBytesInUse(const std::string& key) OVERRIDE;
  virtual size_t GetBytesInUse(const std::vector<std::string>& keys) OVERRIDE;
  virtual size_t GetBytesInUse() OVERRIDE;
  virtual ReadResult Get(const std::string& key) OVERRIDE;
  virtual ReadResult Get(const std::vector<std::string>& keys) OVERRIDE;
  virtual ReadResult Get() OVERRIDE;
  virtual WriteResult Set(
      WriteOptions options,
      const std::string& key,
      const Value& value) OVERRIDE;
  virtual WriteResult Set(
      WriteOptions options, const DictionaryValue& values) OVERRIDE;
  virtual WriteResult Remove(const std::string& key) OVERRIDE;
  virtual WriteResult Remove(const std::vector<std::string>& keys) OVERRIDE;
  virtual WriteResult Clear() OVERRIDE;

 private:
  // The delegate storage area, NOT OWNED.
  ValueStore* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(WeakUnlimitedSettingsStorage);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_WEAK_UNLIMITED_SETTINGS_STORAGE_H_
