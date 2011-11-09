// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/process_map.h"

namespace extensions {

// Item
ProcessMap::Item::Item() {
}

ProcessMap::Item::Item(const ProcessMap::Item& other)
    : extension_id(other.extension_id), process_id(other.process_id) {
}

ProcessMap::Item::Item(const std::string& extension_id, int process_id)
    : extension_id(extension_id), process_id(process_id) {
}

ProcessMap::Item::~Item() {
}

bool ProcessMap::Item::operator<(const ProcessMap::Item& other) const {
  if (extension_id < other.extension_id)
    return true;

  if (extension_id == other.extension_id &&
      process_id < other.process_id) {
    return true;
  }

  return false;
}


// ProcessMap
ProcessMap::ProcessMap() {
}

ProcessMap::~ProcessMap() {
}

bool ProcessMap::Insert(const std::string& extension_id, int process_id) {
  return items_.insert(Item(extension_id, process_id)).second;
}

bool ProcessMap::Remove(const std::string& extension_id, int process_id) {
  return items_.erase(Item(extension_id, process_id)) > 0;
}

int ProcessMap::Remove(int process_id) {
  int result = 0;
  for (ItemSet::const_iterator iter = items_.begin(); iter != items_.end(); ) {
    if (iter->process_id == process_id) {
      items_.erase(iter++);
      ++result;
    } else {
      ++iter;
    }
  }
  return result;
}

bool ProcessMap::Contains(const std::string& extension_id,
                          int process_id) const {
  return items_.find(Item(extension_id, process_id)) != items_.end();
}

bool ProcessMap::Contains(int process_id) const {
  for (ItemSet::const_iterator iter = items_.begin(); iter != items_.end();
       ++iter) {
    if (iter->process_id == process_id)
      return true;
  }
  return false;
}

std::set<std::string> ProcessMap::GetExtensionsInProcess(int process_id) const {
  std::set<std::string> result;
  for (ItemSet::const_iterator iter = items_.begin(); iter != items_.end();
       ++iter) {
    if (iter->process_id == process_id)
      result.insert(iter->extension_id);
  }
  return result;
}

}  // extensions
