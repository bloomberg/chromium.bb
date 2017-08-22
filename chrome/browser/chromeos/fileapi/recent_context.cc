// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_context.h"

#include <utility>

namespace chromeos {

RecentContext::RecentContext() : is_valid_(false), max_files_(0) {}

RecentContext::RecentContext(storage::FileSystemContext* file_system_context,
                             const GURL& origin,
                             size_t max_files,
                             const base::Time& cutoff_time)
    : is_valid_(true),
      file_system_context_(file_system_context),
      origin_(origin),
      max_files_(max_files),
      cutoff_time_(cutoff_time) {}

RecentContext::RecentContext(const RecentContext& other) = default;

RecentContext::RecentContext(RecentContext&& other)
    : is_valid_(other.is_valid_),
      file_system_context_(std::move(other.file_system_context_)),
      origin_(std::move(other.origin_)),
      max_files_(other.max_files_),
      cutoff_time_(std::move(other.cutoff_time_)) {
  other.is_valid_ = false;
}

RecentContext::~RecentContext() = default;

RecentContext& RecentContext::operator=(const RecentContext& other) = default;

}  // namespace chromeos
