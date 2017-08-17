// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_context.h"

#include <utility>

namespace chromeos {

RecentContext::RecentContext() : is_valid_(false) {}

RecentContext::RecentContext(storage::FileSystemContext* file_system_context,
                             const GURL& origin)
    : is_valid_(true),
      file_system_context_(file_system_context),
      origin_(origin) {}

RecentContext::RecentContext(const RecentContext& other) = default;

RecentContext::RecentContext(RecentContext&& other)
    : is_valid_(other.is_valid_),
      file_system_context_(std::move(other.file_system_context_)),
      origin_(std::move(other.origin_)) {
  other.is_valid_ = false;
}

RecentContext::~RecentContext() = default;

RecentContext& RecentContext::operator=(const RecentContext& other) = default;

}  // namespace chromeos
