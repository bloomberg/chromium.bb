// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/process_map.h"

namespace extensions {

// Item
struct ProcessMap::Item {
  Item() {
  }

  explicit Item(const ProcessMap::Item& other)
      : extension_id(other.extension_id),
        process_id(other.process_id),
        site_instance_id(other.site_instance_id) {
  }

  Item(const std::string& extension_id, int process_id,
       int site_instance_id)
      : extension_id(extension_id),
        process_id(process_id),
        site_instance_id(site_instance_id) {
  }

  ~Item() {
  }

  bool operator<(const ProcessMap::Item& other) const {
    if (extension_id < other.extension_id)
      return true;

    if (extension_id == other.extension_id &&
        process_id < other.process_id) {
      return true;
    }

    if (extension_id == other.extension_id &&
        process_id == other.process_id &&
        site_instance_id < other.site_instance_id) {
      return true;
    }

    return false;
  }

  std::string extension_id;
  int process_id;
  int site_instance_id;
};


// ProcessMap
ProcessMap::ProcessMap() {
}

ProcessMap::~ProcessMap() {
}

bool ProcessMap::Insert(const std::string& extension_id, int process_id,
                        int site_instance_id) {
  return items_.insert(Item(extension_id, process_id, site_instance_id)).second;
}

bool ProcessMap::Remove(const std::string& extension_id, int process_id,
                        int site_instance_id) {
  return items_.erase(Item(extension_id, process_id, site_instance_id)) > 0;
}

int ProcessMap::RemoveAllFromProcess(int process_id) {
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
  for (ItemSet::const_iterator iter = items_.begin(); iter != items_.end();
       ++iter) {
    if (iter->process_id == process_id && iter->extension_id == extension_id)
      return true;
  }
  return false;
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
