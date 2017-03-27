// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_

#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

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
    : public base::RefCountedThreadSafe<BackgroundFetchContext,
                                        BrowserThread::DeleteOnUIThread> {
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
  friend class base::DeleteHelper<BackgroundFetchContext>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::RefCountedThreadSafe<BackgroundFetchContext,
                                          BrowserThread::DeleteOnUIThread>;

  ~BackgroundFetchContext();

  void CreateRequest(
      std::unique_ptr<BackgroundFetchJobInfo> job_info,
      std::vector<std::unique_ptr<BackgroundFetchRequestInfo>> request_infos);

  // Callback for the JobController when the job is complete.
  void DidCompleteJob(const std::string& job_guid);

  void ShutdownOnIO();

  // |this| is owned by the BrowserContext via the StoragePartition.
  BrowserContext* browser_context_;
  StoragePartition* storage_partition_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  BackgroundFetchDataManager background_fetch_data_manager_;

  std::unordered_map<std::string, std::unique_ptr<BackgroundFetchJobController>>
      job_map_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_
