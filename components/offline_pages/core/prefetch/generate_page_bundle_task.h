// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GENERATE_PAGE_BUNDLE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GENERATE_PAGE_BUNDLE_TASK_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {
class PrefetchGCMHandler;
class PrefetchNetworkRequestFactory;

// Task that attempts to start archiving the URLs the prefetch service has
// determined are viable to prefetch.
class GeneratePageBundleTask : public Task {
 public:
  // TODO(dewittj): remove the list of prefetch URLs when the DB operation can
  // supply the current set of URLs.
  GeneratePageBundleTask(const std::vector<PrefetchURL>& prefetch_urls,
                         PrefetchGCMHandler* gcm_handler,
                         PrefetchNetworkRequestFactory* request_factory,
                         const PrefetchRequestFinishedCallback& callback);
  ~GeneratePageBundleTask() override;

  // Task implementation.
  void Run() override;

 private:
  void StartGeneratePageBundle(int updated_entry_count);
  void GotRegistrationId(const std::string& id,
                         instance_id::InstanceID::Result result);

  std::vector<PrefetchURL> prefetch_urls_;
  PrefetchGCMHandler* gcm_handler_;
  PrefetchNetworkRequestFactory* request_factory_;
  PrefetchRequestFinishedCallback callback_;

  base::WeakPtrFactory<GeneratePageBundleTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeneratePageBundleTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GENERATE_PAGE_BUNDLE_TASK_H_
