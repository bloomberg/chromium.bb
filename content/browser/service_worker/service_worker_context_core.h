// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace base {
class FilePath;
}

namespace quota {
class QuotaManagerProxy;
}

namespace content {

// This class manages data associated with service workers.
// The class is single threaded and should only be used on the IO thread.
// In chromium, there is one instance per storagepartition. This class
// is the root of the containment hierarchy for service worker data
// associated with a particular partition.
class CONTENT_EXPORT ServiceWorkerContextCore
    : NON_EXPORTED_BASE(
          public base::SupportsWeakPtr<ServiceWorkerContextCore>) {
 public:
  // Given an empty |user_data_directory|, nothing will be stored on disk.
  ServiceWorkerContextCore(const base::FilePath& user_data_directory,
                           quota::QuotaManagerProxy* quota_manager_proxy);
  ~ServiceWorkerContextCore();

  bool IsEnabled();

 private:
  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  base::FilePath path_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
