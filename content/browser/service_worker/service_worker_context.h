// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace base {
class FilePath;
}

namespace quota {
class QuotaManagerProxy;
}

namespace content {

// This class manages metadata associated with all service workers,
// including:
// - persistent storage of pattern -> service worker scripts
// - initialization and initial installation of service workers
// - dispatching of non-fetch events to service workers
class CONTENT_EXPORT ServiceWorkerContext
    : public base::RefCountedThreadSafe<ServiceWorkerContext> {
 public:
  // This is owned by the StoragePartition, which will supply it with
  // the local path on disk.
  ServiceWorkerContext(const base::FilePath& path,
                       quota::QuotaManagerProxy* quota_manager_proxy);

  bool IsEnabled();

 private:
  friend class base::RefCountedThreadSafe<ServiceWorkerContext>;
  ~ServiceWorkerContext();

  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_H_
