// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_PROXY_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_PROXY_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class BackgroundFetchDelegate;
class BackgroundFetchJobController;

// Proxy class for passing messages between BackgroundFetchJobControllers on the
// IO thread and BackgroundFetchDelegate on the UI thread.
class CONTENT_EXPORT BackgroundFetchDelegateProxy {
 public:
  // Subclasses must only be destroyed on the IO thread, since these methods
  // will be called on the IO thread.
  class Controller {
   public:
    // Called when the given |request| has started fetching, after having been
    // assigned the |download_guid| by the download system.
    virtual void DidStartRequest(
        const scoped_refptr<BackgroundFetchRequestInfo>& request,
        const std::string& download_guid) = 0;

    // Called when the given |request| has an update, meaning that a total of
    // |bytes_downloaded| are now available for the response.
    virtual void DidUpdateRequest(
        const scoped_refptr<BackgroundFetchRequestInfo>& request,
        const std::string& download_guid,
        uint64_t bytes_downloaded) = 0;

    // Called when the given |request| has been completed.
    virtual void DidCompleteRequest(
        const scoped_refptr<BackgroundFetchRequestInfo>& request,
        const std::string& download_guid) = 0;

    virtual ~Controller() {}
  };

  explicit BackgroundFetchDelegateProxy(BackgroundFetchDelegate* delegate);

  ~BackgroundFetchDelegateProxy();

  // Creates a new download grouping identified by |job_unique_id|. Further
  // downloads started by StartRequest will also use this |job_unique_id| so
  // that a notification can be updated with the current status. If the download
  // was already started in a previous browser session, then |current_guids|
  // should contain the GUIDs of in progress downloads, while completed
  // downloads are recorded in |completed_parts|.
  // Should only be called from the Controller (on the IO
  // thread).
  void CreateDownloadJob(const std::string& job_unique_id,
                         const std::string& title,
                         const url::Origin& origin,
                         int completed_parts,
                         int total_parts,
                         const std::vector<std::string>& current_guids);

  // Requests that the download manager start fetching |request|.
  // Should only be called from the Controller (on the IO
  // thread).
  void StartRequest(const std::string& job_unique_id,
                    base::WeakPtr<Controller> job_controller,
                    const url::Origin& origin,
                    scoped_refptr<BackgroundFetchRequestInfo> request);

  // Updates the representation of this Background Fetch in the user interface
  // to match the given |title|.
  // Should only be called from the BackgroundFetchJobController (on the IO
  // thread).
  void UpdateUI(const std::string& title);

  // Immediately aborts the job identified by |job_unique_id| by request of the
  // developer. Should only be called from the BackgroundFetchJobController (on
  // the IO thread).
  void Abort(const std::string& job_unique_id);

 private:
  class Core;

  // Called when the download identified by |guid| has succeeded/failed/aborted.
  // Should only be called on the IO thread.
  void OnDownloadComplete(const std::string& guid,
                          std::unique_ptr<BackgroundFetchResult> result);

  // Called when progress has been made for the download identified by |guid|.
  // Should only be called on the IO thread.
  void OnDownloadUpdated(const std::string& guid, uint64_t bytes_downloaded);

  // Should only be called from the BackgroundFetchDelegate (on the IO thread).
  void DidStartRequest(const std::string& guid,
                       std::unique_ptr<BackgroundFetchResponse> response);

  std::unique_ptr<Core, BrowserThread::DeleteOnUIThread> ui_core_;
  base::WeakPtr<Core> ui_core_ptr_;

  // Map from DownloadService GUIDs to the RequestInfo and the JobController
  // that started the download.
  std::map<std::string,
           std::pair<scoped_refptr<BackgroundFetchRequestInfo>,
                     base::WeakPtr<Controller>>>
      controller_map_;

  base::WeakPtrFactory<BackgroundFetchDelegateProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDelegateProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_PROXY_H_
