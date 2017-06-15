// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_SCHEDULER_SCHEDULER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_SCHEDULER_SCHEDULER_H_

#include "components/download/internal/model.h"
#include "components/download/internal/scheduler/device_status.h"
#include "components/download/public/download_params.h"

namespace download {

struct DeviceStatus;

// The interface that talks to download service to schedule platform background
// download tasks.
class Scheduler {
 public:
  // Reschedule another background platform task. Called when downloads are
  // added or removed or the criteria has changed.
  virtual void Reschedule(const Model::EntryList& entries) = 0;

  // Returns the next download that should be processed based on scheduling
  // parameters, may return nullptr if no download meets the criteria.
  // The sequence of polling on entries with exactly same states is undefined.
  virtual Entry* Next(const Model::EntryList& entries,
                      const DeviceStatus& device_status) = 0;

  virtual ~Scheduler() {}
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_CORE_SCHEDULER_H_
