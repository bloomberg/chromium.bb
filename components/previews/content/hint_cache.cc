// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/hint_cache.h"

#include "url/gurl.h"

namespace previews {

// Realistic minimum length of a host suffix.
const int kMinHostSuffix = 6;  // eg., abc.tv

HintCache::Data::Data() = default;

HintCache::Data::~Data() = default;

void HintCache::Data::AddHint(const optimization_guide::proto::Hint& hint) {
  DCHECK_EQ(optimization_guide::proto::HOST_SUFFIX, hint.key_representation());
  activation_list_.insert(hint.key());
  memory_cache_[hint.key()] = hint;
}

bool HintCache::Data::HasHints() const {
  return !activation_list_.empty();
}

HintCache::Data::Data(Data&& other) = default;

HintCache::HintCache(Data&& data) : data_(std::move(data)) {
  // Detach from sequence as the HintCache can be constructed on a different
  // thread than it is used on.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

bool HintCache::HasHint(const std::string& host) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !DetermineHostSuffix(host).empty();
}

void HintCache::LoadHint(const std::string& host, HintLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const optimization_guide::proto::Hint* hint = GetHint(host);
  if (hint) {
    // Hint already loaded in memory.
    std::move(callback).Run(*hint);
  }
  // TODO(dougarnett): Add backing store support to load from.
}

bool HintCache::IsHintLoaded(const std::string& host) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string host_suffix = DetermineHostSuffix(host);
  if (host_suffix.empty()) {
    return false;
  }
  return data_.memory_cache_.find(host_suffix) != data_.memory_cache_.end();
}

const optimization_guide::proto::Hint* HintCache::GetHint(
    const std::string& host) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string host_suffix = DetermineHostSuffix(host);
  if (host_suffix.empty()) {
    return nullptr;
  }
  auto it = data_.memory_cache_.find(host_suffix);
  if (it != data_.memory_cache_.end()) {
    return &it->second;
  }

  return nullptr;
}

std::string HintCache::DetermineHostSuffix(const std::string& host) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string suffix(host);
  // Look for longest host name suffix that has a hint. No need to continue
  // lookups and substring work once get to a root domain like ".com" or
  // ".co.in" (MinHostSuffix length check is a heuristic for that).
  while (suffix.length() >= kMinHostSuffix) {
    if (data_.activation_list_.find(suffix) != data_.activation_list_.end()) {
      return suffix;
    }
    size_t pos = suffix.find_first_of('.');
    if (pos == std::string::npos) {
      return std::string();
    }
    suffix = suffix.substr(pos + 1);
  }
  return std::string();
}

}  // namespace previews
