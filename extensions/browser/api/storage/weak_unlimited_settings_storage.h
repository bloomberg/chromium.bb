// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_WEAK_UNLIMITED_SETTINGS_STORAGE_H_
#define EXTENSIONS_BROWSER_API_STORAGE_WEAK_UNLIMITED_SETTINGS_STORAGE_H_

#include "base/compiler_specific.h"
#include "extensions/browser/value_store/value_store.h"

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
  virtual size_t GetBytesInUse(const std::string& key) override;
  virtual size_t GetBytesInUse(const std::vector<std::string>& keys) override;
  virtual size_t GetBytesInUse() override;
  virtual ReadResult Get(const std::string& key) override;
  virtual ReadResult Get(const std::vector<std::string>& keys) override;
  virtual ReadResult Get() override;
  virtual WriteResult Set(
      WriteOptions options,
      const std::string& key,
      const base::Value& value) override;
  virtual WriteResult Set(
      WriteOptions options, const base::DictionaryValue& values) override;
  virtual WriteResult Remove(const std::string& key) override;
  virtual WriteResult Remove(const std::vector<std::string>& keys) override;
  virtual WriteResult Clear() override;
  virtual bool Restore() override;
  virtual bool RestoreKey(const std::string& key) override;

 private:
  // The delegate storage area, NOT OWNED.
  ValueStore* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(WeakUnlimitedSettingsStorage);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_WEAK_UNLIMITED_SETTINGS_STORAGE_H_
