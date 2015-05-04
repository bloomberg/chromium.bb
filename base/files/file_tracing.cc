// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_tracing.h"

#include "base/files/file.h"
#include "base/files/file_path.h"

namespace base {

const char kFileTracingEventCategoryGroup[] = TRACE_DISABLED_BY_DEFAULT("file");

ScopedFileTrace::ScopedFileTrace() : initialized_(false) {}

ScopedFileTrace::~ScopedFileTrace() {
  if (initialized_) {
    if (size_) {
      TRACE_EVENT_NESTABLE_ASYNC_END2(
          kFileTracingEventCategoryGroup, name_, &file_->trace_enabler_,
          "path", file_->path_.AsUTF8Unsafe(), "size", size_);
    } else {
      TRACE_EVENT_NESTABLE_ASYNC_END1(
          kFileTracingEventCategoryGroup, name_, &file_->trace_enabler_,
          "path", file_->path_.AsUTF8Unsafe());
    }
  }
}

void ScopedFileTrace::Initialize(const char* name, File* file, int64 size) {
  file_ = file;
  name_ = name;
  size_ = size;
  initialized_ = true;

  if (size_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
        kFileTracingEventCategoryGroup, name_, &file_->trace_enabler_,
        "path", file_->path_.AsUTF8Unsafe(), "size", size_);
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        kFileTracingEventCategoryGroup, name_, &file_->trace_enabler_,
        "path", file_->path_.AsUTF8Unsafe());
  }
}

}  // namespace base
