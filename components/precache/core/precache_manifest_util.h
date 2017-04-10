// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_MANIFEST_UTIL_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_MANIFEST_UTIL_H_

#include <stdint.h>

#include <vector>

#include "base/optional.h"

namespace precache {

class PrecacheManifest;

// Removes unknown fields from the |manifest| including embedded messages.
void RemoveUnknownFields(PrecacheManifest* manifest);

// Returns the resource selection bitset from the |manifest| for the given
// |experiment_id|. If the experiment group is not found, then this returns
// nullopt, in which case all resources should be selected.
base::Optional<std::vector<bool>> GetResourceBitset(
    const PrecacheManifest& manifest,
    uint32_t experiment_id);

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CORE_PRECACHE_MANIFEST_UTIL_H_
