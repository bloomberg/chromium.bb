// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/tile_task_runner.h"
#include "cc/resources/platform_color.h"

namespace cc {

TileTask::TileTask(bool supports_concurrent_execution)
    : supports_concurrent_execution_(supports_concurrent_execution),
      did_schedule_(false),
      did_complete_(false) {}

TileTask::TileTask(bool supports_concurrent_execution,
                   TileTask::Vector* dependencies)
    : supports_concurrent_execution_(supports_concurrent_execution),
      dependencies_(std::move(*dependencies)),
      did_schedule_(false),
      did_complete_(false) {}

TileTask::~TileTask() {
  DCHECK(!did_schedule_);
  DCHECK(!did_run_ || did_complete_);
}

void TileTask::WillSchedule() {
  DCHECK(!did_schedule_);
}

void TileTask::DidSchedule() {
  did_schedule_ = true;
  did_complete_ = false;
}

bool TileTask::HasBeenScheduled() const {
  return did_schedule_;
}

void TileTask::WillComplete() {
  DCHECK(!did_complete_);
}

void TileTask::DidComplete() {
  DCHECK(did_schedule_);
  DCHECK(!did_complete_);
  did_schedule_ = false;
  did_complete_ = true;
}

bool TileTask::HasCompleted() const {
  return did_complete_;
}

bool TileTaskRunner::ResourceFormatRequiresSwizzle(ResourceFormat format) {
  switch (format) {
    case RGBA_8888:
    case BGRA_8888:
      // Initialize resource using the preferred PlatformColor component
      // order and swizzle in the shader instead of in software.
      return !PlatformColor::SameComponentOrder(format);
    case RGBA_4444:
    case ETC1:
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case RED_8:
    case LUMINANCE_F16:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace cc
