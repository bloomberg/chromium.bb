// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_

#include "base/callback.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_single_script_update_checker.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

class ServiceWorkerVersion;

class ServiceWorkerUpdateChecker {
 public:
  // Data of each compared script needed in remaining update process
  struct ComparedScriptInfo {
    ComparedScriptInfo();
    ComparedScriptInfo(
        int64_t old_resource_id,
        ServiceWorkerSingleScriptUpdateChecker::Result result,
        std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
            paused_state);
    ComparedScriptInfo(ComparedScriptInfo&& other);
    ComparedScriptInfo& operator=(ComparedScriptInfo&& other);
    ~ComparedScriptInfo();

    // Resource id of the compared script already in storage
    int64_t old_resource_id;

    // Compare result of a single script
    ServiceWorkerSingleScriptUpdateChecker::Result result;

    // This is only valid for script compared to be different. It consists of
    // some state variables to continue the update process, including
    // ServiceWorkerCacheWriter, and Mojo endpoints for downloading.
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
        paused_state;
  };

  using UpdateStatusCallback = base::OnceCallback<void(bool)>;

  ServiceWorkerUpdateChecker(
      std::vector<ServiceWorkerDatabase::ResourceRecord> scripts_to_compare,
      const GURL& main_script_url,
      int64_t main_script_resource_id,
      scoped_refptr<ServiceWorkerVersion> version_to_update,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);
  ~ServiceWorkerUpdateChecker();

  // |callback| is always triggered when Start() finishes. If the scripts are
  // found to have any changes, the argument of |callback| is true and otherwise
  // false.
  void Start(UpdateStatusCallback callback);

  // This transfers the ownership of the check result to the caller. It only
  // contains the information about scripts which have already been compared.
  std::map<GURL, ComparedScriptInfo> TakeComparedResults();

  void OnOneUpdateCheckFinished(
      const GURL& script_url,
      int64_t old_resource_id,
      ServiceWorkerSingleScriptUpdateChecker::Result result,
      std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
          paused_state);

 private:
  void CheckOneScript(const GURL& url, const int64_t resource_id);

  std::vector<ServiceWorkerDatabase::ResourceRecord> scripts_to_compare_;
  size_t next_script_index_to_compare_ = 0;
  std::map<GURL, ComparedScriptInfo> script_check_results_;
  const GURL main_script_url_;
  const int64_t main_script_resource_id_;

  // The version which triggered this update.
  scoped_refptr<ServiceWorkerVersion> version_to_update_;

  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> running_checker_;

  UpdateStatusCallback callback_;

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  base::WeakPtrFactory<ServiceWorkerUpdateChecker> weak_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceWorkerUpdateChecker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_
