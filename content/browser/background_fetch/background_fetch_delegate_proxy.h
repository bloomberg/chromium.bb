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
#include "base/optional.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {

class BrowserContext;

// Proxy class for passing messages between BackgroundFetchJobControllers on the
// IO thread and BackgroundFetchDelegate on the UI thread.
class CONTENT_EXPORT BackgroundFetchDelegateProxy {
 public:
  // Subclasses must only be destroyed on the IO thread, since these methods
  // will be called on the IO thread.
  using DispatchClickEventCallback =
      base::RepeatingCallback<void(const std::string& unique_id)>;
  class Controller {
   public:
    // Called when the given |request| has started fetching.
    virtual void DidStartRequest(
        const scoped_refptr<BackgroundFetchRequestInfo>& request) = 0;

    // Called when the given |request| has an update, meaning that a total of
    // |bytes_uploaded| of the request were uploaded, and a total of
    // |bytes_downloaded| are now available for the response.
    virtual void DidUpdateRequest(
        const scoped_refptr<BackgroundFetchRequestInfo>& request,
        uint64_t bytes_uploaded,
        uint64_t bytes_downloaded) = 0;

    // Called when the given |request| has been completed.
    virtual void DidCompleteRequest(
        const scoped_refptr<BackgroundFetchRequestInfo>& request) = 0;

    // Called when the delegate aborts a Background Fetch registration.
    virtual void AbortFromDelegate(
        blink::mojom::BackgroundFetchFailureReason) = 0;

    // Called by the delegate when the Download Service is requesting the
    // upload data.
    virtual void GetUploadData(
        const scoped_refptr<BackgroundFetchRequestInfo>& request,
        BackgroundFetchDelegate::GetUploadDataCallback callback) = 0;

    virtual ~Controller() {}
  };

  explicit BackgroundFetchDelegateProxy(BrowserContext* browser_context);

  ~BackgroundFetchDelegateProxy();

  // Set BackgroundFetchClick event dispatcher callback, which is a method on
  // the background fetch context.
  void SetClickEventDispatcher(
      const DispatchClickEventCallback click_event_callback);

  // Gets size of the icon to display with the Background Fetch UI.
  void GetIconDisplaySize(
      BackgroundFetchDelegate::GetIconDisplaySizeCallback callback);

  // Checks if the provided origin has permission to start a Background Fetch.
  void GetPermissionForOrigin(
      const url::Origin& origin,
      const ResourceRequestInfo::WebContentsGetter& wc_getter,
      BackgroundFetchDelegate::GetPermissionForOriginCallback callback);

  // Creates a new download grouping described by |fetch_description|. Further
  // downloads started by StartRequest will also use
  // |fetch_description.job_unique_id| so that a notification can be updated
  // with the current status. If the download was already started in a previous
  // browser session, then |fetch_description.current_guids| should contain the
  // GUIDs of in progress downloads, while completed downloads are recorded in
  // |fetch_description.completed_parts|. The size of the completed parts is
  // recorded in |fetch_description.completed_parts_size| and total download
  // size is stored in |fetch_description.total_parts_size|.
  // |active_fetch_requests| contains the BackgroundFetchRequestInfos
  // needed to correctly resume an ongoing fetch.
  // Should only be called from the Controller (on the IO thread).
  void CreateDownloadJob(
      base::WeakPtr<Controller> controller,
      std::unique_ptr<BackgroundFetchDescription> fetch_description,
      std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
          active_fetch_requests);

  // Requests that the download manager start fetching |request|.
  // Should only be called from the Controller (on the IO
  // thread).
  void StartRequest(const std::string& job_unique_id,
                    const url::Origin& origin,
                    scoped_refptr<BackgroundFetchRequestInfo> request);

  // Updates the representation of this registration in the user interface to
  // match the given |title| or |icon|. |update_ui_callback| should be called
  // after all the relevant UI information has been processed.
  // Called from the Controller (on the IO thread).
  void UpdateUI(const std::string& job_unique_id,
                const base::Optional<std::string>& title,
                const base::Optional<SkBitmap>& icon,
                blink::mojom::BackgroundFetchService::UpdateUICallback
                    update_ui_callback);

  // Aborts in progress downloads for the given registration. Called from the
  // Controller (on the IO thread) after it is aborted. May occur even if all
  // requests already called OnDownloadComplete.
  void Abort(const std::string& job_unique_id);

  // Called when the fetch associated |job_unique_id| is completed.
  void MarkJobComplete(const std::string& job_unique_id);

 private:
  class Core;

  // Called when the job identified by |job_unique|id| was cancelled by the
  // delegate. Should only be called on the IO thread.
  void OnJobCancelled(
      const std::string& job_unique_id,
      blink::mojom::BackgroundFetchFailureReason reason_to_abort);

  // Called when the download identified by |guid| has succeeded/failed/aborted.
  // Should only be called on the IO thread.
  void OnDownloadComplete(const std::string& job_unique_id,
                          const std::string& guid,
                          std::unique_ptr<BackgroundFetchResult> result);

  // Called when progress has been made for the download identified by |guid|.
  // Progress is either the request body being uploaded or response body being
  // downloaded. Should only be called on the IO thread.
  void OnDownloadUpdated(const std::string& job_unique_id,
                         const std::string& guid,
                         uint64_t bytes_uploaded,
                         uint64_t bytes_downloaded);

  // Should only be called from the BackgroundFetchDelegate (on the IO thread).
  void DidStartRequest(const std::string& job_unique_id,
                       const std::string& guid,
                       std::unique_ptr<BackgroundFetchResponse> response);

  // Should only be called from the BackgroundFetchDelegate (on the IO thread).
  void DidActivateUI(const std::string& job_unique_id);

  // Should only be called from the BackgroundFetchDelegate (on the IO thread).
  void DidUpdateUI(const std::string& job_unique_id);

  // Should only be called from the BackgroundFetchDelegate (on the IO thread).
  void GetUploadData(const std::string& job_unique_id,
                     const std::string& download_guid,
                     BackgroundFetchDelegate::GetUploadDataCallback callback);

  std::unique_ptr<Core, BrowserThread::DeleteOnUIThread> ui_core_;
  base::WeakPtr<Core> ui_core_ptr_;

  struct JobDetails {
    JobDetails(base::WeakPtr<Controller> controller,
               std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
                   active_fetch_requests);
    JobDetails(JobDetails&& details);
    ~JobDetails();

    base::WeakPtr<Controller> controller;

    // Map from DownloadService GUIDs to their corresponding request.
    base::flat_map<std::string, scoped_refptr<BackgroundFetchRequestInfo>>
        current_request_map;

    blink::mojom::BackgroundFetchService::UpdateUICallback update_ui_callback;

   private:
    DISALLOW_COPY_AND_ASSIGN(JobDetails);
  };

  // Map from unique job ids to a JobDetails containing the outstanding download
  // GUIDs and the controller that started the download.
  std::map<std::string, JobDetails> job_details_map_;

  DispatchClickEventCallback click_event_dispatcher_callback_;
  base::WeakPtrFactory<BackgroundFetchDelegateProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDelegateProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_PROXY_H_
