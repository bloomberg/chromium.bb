// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MTPDeviceOperationsUtils provides support to media transfer protocol device
// file operation worker classes.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_OPERATIONS_UTILS_H_
#define CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_OPERATIONS_UTILS_H_

#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/threading/sequenced_worker_pool.h"

namespace device {
class MediaTransferProtocolManager;
}

namespace chrome {

// Helper struct to delete worker objects on |media_task_runner_| thread.
template <typename WORKER> struct WorkerDeleter {
  static void Destruct(const WORKER* worker) {
    if (!worker->media_task_runner()->RunsTasksOnCurrentThread()) {
      worker->media_task_runner()->DeleteSoon(FROM_HERE, worker);
      return;
    }
    delete worker;
  }
};

// Returns MediaTransferProtocolManager instance on success or NULL on failure.
device::MediaTransferProtocolManager* GetMediaTransferProtocolManager();

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_OPERATIONS_UTILS_H_
