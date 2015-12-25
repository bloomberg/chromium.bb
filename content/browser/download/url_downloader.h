// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_
#define CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_

#include <stdint.h>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/download/download_request_core.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/common/referrer.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"

namespace content {
class DownloadManagerImpl;

class UrlDownloader : public net::URLRequest::Delegate {
 public:
  UrlDownloader(
      scoped_ptr<net::URLRequest> request,
      base::WeakPtr<DownloadManagerImpl> manager,
      scoped_ptr<DownloadSaveInfo> save_info,
      uint32_t download_id,
      const DownloadUrlParameters::OnStartedCallback& on_started_callback);
  ~UrlDownloader() override;

  static scoped_ptr<UrlDownloader> BeginDownload(
      base::WeakPtr<DownloadManagerImpl> download_manager,
      scoped_ptr<net::URLRequest> request,
      const Referrer& referrer,
      bool prefer_cache,
      scoped_ptr<DownloadSaveInfo> save_info,
      uint32_t download_id,
      const DownloadUrlParameters::OnStartedCallback& started_callback);

  // URLRequest::Delegate:
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  void StartReading(bool is_continuation);
  void ResponseCompleted();

  void Start();
  void ResumeReading();

  void CallStartedCallbackOnFailure(DownloadInterruptReason result);

 private:
  class RequestHandle;

  void PauseRequest();
  void ResumeRequest();
  void CancelRequest();

  scoped_ptr<net::URLRequest> request_;
  base::WeakPtr<DownloadManagerImpl> manager_;
  uint32_t download_id_;
  DownloadUrlParameters::OnStartedCallback on_started_callback_;

  DownloadRequestCore handler_;

  base::WeakPtrFactory<UrlDownloader> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_
