// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_PROXY_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_PROXY_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/public/browser/browser_thread.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {

class BackgroundFetchJobController;
struct BackgroundFetchResponse;
class BrowserContext;

// Proxy class for passing messages between BackgroundFetchJobControllers on the
// IO thread and BackgroundFetchDelegate on the UI thread.
// TODO(delphick): Create BackgroundFetchDelegate.
class CONTENT_EXPORT BackgroundFetchDelegateProxy {
 public:
  BackgroundFetchDelegateProxy(
      BrowserContext* browser_context,
      scoped_refptr<net::URLRequestContextGetter> request_context);

  ~BackgroundFetchDelegateProxy();

  // Requests that the download manager start fetching |request|.
  // Should only be called from the BackgroundFetchJobController (on the IO
  // thread).
  void StartRequest(BackgroundFetchJobController* job_controller,
                    scoped_refptr<BackgroundFetchRequestInfo> request);

  // Updates the representation of this Background Fetch in the user interface
  // to match the given |title|.
  // Should only be called from the BackgroundFetchJobController (on the IO
  // thread).
  void UpdateUI(const std::string& title);

  // Immediately aborts this Background Fetch by request of the developer.
  // Should only be called from the BackgroundFetchJobController (on the IO
  // thread).
  void Abort();

 private:
  class Core;

  // Called when the given download identified by |guid| has been completed.
  // Should only be called on the IO thread.
  void OnDownloadComplete(const std::string& guid,
                          std::unique_ptr<BackgroundFetchResult> result);

  // Should only be called from the BackgroundFetchDelegate (on the IO thread).
  void DidStartRequest(const std::string& guid,
                       std::unique_ptr<BackgroundFetchResponse> response);

  std::unique_ptr<Core, BrowserThread::DeleteOnUIThread> ui_core_;
  base::WeakPtr<Core> ui_core_ptr_;

  // Map from DownloadService GUIDs to the RequestInfo and the JobController
  // that started the download.
  std::map<std::string,
           std::pair<scoped_refptr<BackgroundFetchRequestInfo>,
                     base::WeakPtr<BackgroundFetchJobController>>>
      controller_map_;

  base::WeakPtrFactory<BackgroundFetchDelegateProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDelegateProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_PROXY_H_
