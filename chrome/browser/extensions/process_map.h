// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PROCESS_MAP_H_
#define CHROME_BROWSER_EXTENSIONS_PROCESS_MAP_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"

namespace extensions {

// Contains information about which extensions are assigned to which processes.
//
// The relationship between extensions and processes is complex:
// - Extensions can be either "split" mode or "spanning" mode.
// - In spanning mode, extensions share a single process between all incognito
//   and normal windows. This was the original mode for extensions.
// - In split mode, extensions have separate processes in incognito windows.
// - There are also hosted apps, which are a kind of extensions, and those
//   usually have a process model similar to normal web sites: multiple
//   processes per-profile.
//
// In general, we seem to play with the process model of extensions a lot, so
// it is safest to assume it is many-to-many in most places in the codebase.
//
// Note that because of content scripts, frames, and other edge cases in
// Chrome's process isolation, extension code can still end up running outside
// an assigned process.
//
// But we only allow high-privilege operations to be performed by an extension
// when it is running in an assigned process.
class ProcessMap {
 public:
  ProcessMap();
  ~ProcessMap();

  size_t size() const { return items_.size(); }

  bool Insert(const std::string& extension_id, int process_id);
  bool Remove(const std::string& extension_id, int process_id);
  int Remove(int process_id);
  bool Contains(const std::string& extension_id, int process_id) const;
  bool Contains(int process_id) const;

  std::set<std::string> GetExtensionsInProcess(int process_id) const;

 private:
  struct Item {
    Item();
    Item(const Item& other);
    Item(const std::string& extension_id, int process_id);
    ~Item();

    // Required for set membership.
    bool operator<(const Item& other) const;

    std::string extension_id;
    int process_id;
  };

  typedef std::set<Item> ItemSet;
  std::set<Item> items_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMap);
};

}  // extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PROCESS_MAP_H_
