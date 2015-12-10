// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_
#define CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_

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
class DownloadRequestCore;

class UrlDownloader : public net::URLRequest::Delegate {
 public:
  UrlDownloader(scoped_ptr<net::URLRequest> request,
                scoped_ptr<DownloadRequestCore> handler,
                base::WeakPtr<DownloadManagerImpl> manager);
  ~UrlDownloader() override;

  static scoped_ptr<UrlDownloader> BeginDownload(
      base::WeakPtr<DownloadManagerImpl> download_manager,
      scoped_ptr<net::URLRequest> request,
      const Referrer& referrer,
      bool is_content_initiated,
      bool prefer_cache,
      bool do_not_prompt_for_login,
      scoped_ptr<DownloadSaveInfo> save_info,
      uint32 download_id,
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

 private:
  scoped_ptr<net::URLRequest> request_;
  scoped_ptr<DownloadRequestCore> handler_;
  base::WeakPtr<DownloadManagerImpl> manager_;

  base::WeakPtrFactory<UrlDownloader> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_
