// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

class GURL;

namespace net {
class HttpRequestHeaders;
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace content {
struct BackgroundFetchResponse;
struct BackgroundFetchResult;

// Interface for launching background fetches. Implementing classes would
// generally interface with the DownloadService or DownloadManager.
// Must only be used on the UI thread and generally expected to be called by the
// BackgroundFetchDelegateProxy.
class CONTENT_EXPORT BackgroundFetchDelegate {
 public:
  // Client interface that a BackgroundFetchDelegate would use to signal the
  // progress of a background fetch.
  class Client {
   public:
    virtual ~Client() {}

    // Called after the download has started with the initial response
    // (including headers and URL chain). Always called on the UI thread.
    virtual void OnDownloadStarted(
        const std::string& guid,
        std::unique_ptr<content::BackgroundFetchResponse> response) = 0;

    // Called during the download to indicate the current progress. Always
    // called on the UI thread.
    virtual void OnDownloadUpdated(const std::string& guid,
                                   uint64_t bytes_downloaded) = 0;

    // Called after the download has completed giving the result including the
    // path to the downloaded file and its size. Always called on the UI thread.
    virtual void OnDownloadComplete(
        const std::string& guid,
        std::unique_ptr<BackgroundFetchResult> result) = 0;

    // Called by the delegate when it's shutting down to signal that the
    // delegate is no longer valid.
    virtual void OnDelegateShutdown() = 0;
  };

  BackgroundFetchDelegate();

  virtual ~BackgroundFetchDelegate();

  // Creates a new download grouping identified by |job_unique_id|. Further
  // downloads started by DownloadUrl will also use this |job_unique_id| so that
  // a notification can be updated with the current status. If the download was
  // already started in a previous browser session, then |current_guids| should
  // contain the GUIDs of in progress downloads, while completed downloads are
  // recorded in |completed_parts|.
  virtual void CreateDownloadJob(
      const std::string& job_unique_id,
      const std::string& title,
      const url::Origin& origin,
      int completed_parts,
      int total_parts,
      const std::vector<std::string>& current_guids) = 0;

  // Creates a new download identified by |guid| in the download job identified
  // by |job_unique_id|.
  virtual void DownloadUrl(
      const std::string& job_unique_id,
      const std::string& guid,
      const std::string& method,
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const net::HttpRequestHeaders& headers) = 0;

  // Set the client that the delegate should communicate changes to.
  void SetDelegateClient(base::WeakPtr<Client> client) { client_ = client; }

  base::WeakPtr<Client> client() { return client_; }

 private:
  base::WeakPtr<Client> client_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DELEGATE_H_
