// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_HASH_FILTER_H_
#define CHROME_BROWSER_PREFS_PREF_HASH_FILTER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_filter.h"

class PrefHashStore;

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

// Intercepts preference values as they are loaded from disk and verifies them
// using a PrefHashStore. Keeps the PrefHashStore contents up to date as values
// are changed.
class PrefHashFilter : public PrefFilter {
 public:
  explicit PrefHashFilter(scoped_ptr<PrefHashStore> pref_hash_store);
  virtual ~PrefHashFilter();

  // PrefFilter implementation.
  virtual void FilterOnLoad(base::DictionaryValue* pref_store_contents)
      OVERRIDE;
  virtual void FilterUpdate(const std::string& path,
                            const base::Value* value) OVERRIDE;

 private:
  scoped_ptr<PrefHashStore> pref_hash_store_;
  std::set<std::string> tracked_paths_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashFilter);
};

#endif  // CHROME_BROWSER_PREFS_PREF_HASH_FILTER_H_
