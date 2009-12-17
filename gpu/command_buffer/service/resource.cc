// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of ResourceMapBase.

#include "gpu/command_buffer/service/precompile.h"
#include "gpu/command_buffer/service/resource.h"

namespace gpu {

// Assigns a resource to a resource ID, by setting it at the right location
// into the list, resizing the list if necessary, and destroying an existing
// resource if one existed already.
void ResourceMapBase::Assign(ResourceId id, Resource *resource) {
  if (id >= resources_.size()) {
    resources_.resize(id + 1, NULL);
  } else {
    Resource *&entry = resources_[id];
    if (entry) {
      delete entry;
      entry = NULL;
    }
  }
  DCHECK(resources_[id] == NULL);
  resources_[id] = resource;
}

// Destroys a resource contained in the map, setting its entry to NULL. If
// necessary, this will trim the list.
bool ResourceMapBase::Destroy(ResourceId id) {
  if (id >= resources_.size()) {
    return false;
  }
  Resource *&entry = resources_[id];
  if (entry) {
    delete entry;
    entry = NULL;

    // Removing the last element, we can trim the list.
    // TODO: this may not be optimal to do every time. Investigate if it
    // becomes an issue, and add a threshold before we resize.
    if (id == resources_.size() - 1) {
      size_t last_valid = resources_.max_size();
      for (unsigned int i = id; i < resources_.size(); --i) {
        if (resources_[i]) {
          last_valid = i;
          break;
        }
      }
      if (last_valid == resources_.max_size()) {
        resources_.clear();
      } else {
        resources_.resize(last_valid + 1);
      }
    }
    return true;
  }
  return false;
}

// Goes over all non-NULL entries in the list, destroying them, then clears the
// list.
void ResourceMapBase::DestroyAllResources() {
  for (Container::iterator i = resources_.begin(); i != resources_.end(); ++i) {
    if (*i) {
      delete *i;
    }
  }
  resources_.clear();
}

}  // namespace gpu
