// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_manifest_util.h"

#include <string>

#include "components/precache/core/proto/precache.pb.h"

namespace precache {

void RemoveUnknownFields(PrecacheManifest* manifest) {
  manifest->mutable_unknown_fields()->clear();
  for (auto& resource : *manifest->mutable_resource())
    resource.mutable_unknown_fields()->clear();
  if (manifest->has_experiments()) {
    manifest->mutable_experiments()->mutable_unknown_fields()->clear();
    for (auto& kv : *manifest->mutable_experiments()
                         ->mutable_resources_by_experiment_group()) {
      kv.second.mutable_unknown_fields()->clear();
    }
  }
  if (manifest->has_id())
    manifest->mutable_id()->mutable_unknown_fields()->clear();
}

base::Optional<std::vector<bool>> GetResourceBitset(
    const PrecacheManifest& manifest,
    uint32_t experiment_id) {
  base::Optional<std::vector<bool>> ret;
  if (manifest.has_experiments()) {
    const auto& resource_bitset_map =
        manifest.experiments().resources_by_experiment_group();
    const auto& it = resource_bitset_map.find(experiment_id);
    if (it != resource_bitset_map.end()) {
      if (it->second.has_bitset()) {
        const std::string& bitset = it->second.bitset();
        const int bitset_size = bitset.size() * 8;
        DCHECK_GE(bitset_size, manifest.resource_size());
        if (bitset_size >= manifest.resource_size()) {
          ret.emplace(bitset_size);
          for (size_t i = 0; i < bitset.size(); ++i) {
            for (size_t j = 0; j < 8; ++j) {
              if ((1 << j) & bitset[i])
                ret.value()[i * 8 + j] = true;
            }
          }
        }
      } else if (it->second.has_deprecated_bitset()) {
        uint64_t bitset = it->second.deprecated_bitset();
        DCHECK_GE(64, manifest.resource_size());
        if (64 >= manifest.resource_size()) {
          ret.emplace(64);
          for (int i = 0; i < 64; ++i) {
            if ((0x1ULL << i) & bitset)
              ret.value()[i] = true;
          }
        }
      }
    }
  }
  // Only return one variable to ensure RVO triggers.
  return ret;
}

}  // namespace precache
