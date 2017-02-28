// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/common/content_export.h"

namespace content {

class BackgroundFetchJobInfo;
class BackgroundFetchRequestInfo;
class BrowserContext;
class ServiceWorkerContextWrapper;

// The BackgroundFetchContext is the central moderator of ongoing background
// fetch requests from the Mojo service and from other callers.
// Background Fetch requests function similar to normal fetches except that
// they are persistent across Chromium or service worker shutdown.
class CONTENT_EXPORT BackgroundFetchContext
    : public base::RefCountedThreadSafe<BackgroundFetchContext> {
 public:
  // The BackgroundFetchContext will watch the ServiceWorkerContextWrapper so
  // that it can respond to service worker events such as unregister.
  BackgroundFetchContext(
      BrowserContext* browser_context,
      StoragePartition* storage_partition,
      const scoped_refptr<ServiceWorkerContextWrapper>& context);

  // Init and Shutdown are for use on the UI thread when the StoragePartition is
  // being setup and torn down.
  void Init();

  // Shutdown must be called before deleting this. Call on the UI thread.
  void Shutdown();

  BackgroundFetchDataManager* GetDataManagerForTesting() {
    return &background_fetch_data_manager_;
  }

 private:
  void CreateRequest(const BackgroundFetchJobInfo& job_info,
                     std::vector<BackgroundFetchRequestInfo>& request_infos);

  friend class base::RefCountedThreadSafe<BackgroundFetchContext>;
  ~BackgroundFetchContext();

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  BackgroundFetchJobController background_fetch_job_controller_;
  BackgroundFetchDataManager background_fetch_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_
