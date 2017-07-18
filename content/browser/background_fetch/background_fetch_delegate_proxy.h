// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_PROXY_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_PROXY_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/public/browser/browser_thread.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {

class BackgroundFetchJobController;
class BrowserContext;

// Proxy class for passing messages between BackgroundFetchJobController on the
// IO thread to and BackgroundFetchDelegate on the UI thread.
// TODO(delphick): Create BackgroundFetchDelegate.
class BackgroundFetchDelegateProxy {
 public:
  BackgroundFetchDelegateProxy(
      BackgroundFetchJobController* job_controller,
      const BackgroundFetchRegistrationId& registration_id,
      BrowserContext* browser_context,
      scoped_refptr<net::URLRequestContextGetter> request_context);

  ~BackgroundFetchDelegateProxy();

  // Requests that the download manager start fetching |request|.
  // Should only be called from the BackgroundFetchJobController (on the IO
  // thread).
  void StartRequest(scoped_refptr<BackgroundFetchRequestInfo> request);

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

  // Called when the download manager has started the given |request|. The
  // |download_item| continues to be owned by the download system. The
  // |interrupt_reason| will indicate when a request could not be started.
  // Should only be called from the BackgroundFetchDelegate (on the IO thread).
  void DidStartRequest(scoped_refptr<BackgroundFetchRequestInfo> request,
                       const std::string& download_guid);

  // Called when the given |request| has been completed.
  // Should only be called from the BackgroundFetchDelegate (on the IO thread).
  void DidCompleteRequest(scoped_refptr<BackgroundFetchRequestInfo> request);

  // Parent job controller that owns |this|.
  BackgroundFetchJobController* const job_controller_;

  std::unique_ptr<Core, BrowserThread::DeleteOnUIThread> ui_core_;
  base::WeakPtr<Core> ui_core_ptr_;

  // Traffic annotation for network request.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  base::WeakPtrFactory<BackgroundFetchDelegateProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDelegateProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_PROXY_H_
