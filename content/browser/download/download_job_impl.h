// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_IMPL_H_

#include "base/macros.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_job.h"
#include "content/browser/download/download_request_handle.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT DownloadJobImpl : public DownloadJob {
 public:
  DownloadJobImpl(
      DownloadItemImpl* download_item,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle);
  ~DownloadJobImpl() override;

  // DownloadJob implementation.
  void Start() override;
  void Cancel(bool user_cancel) override;
  void Pause() override;
  void Resume(bool resume_request) override;
  WebContents* GetWebContents() const override;

 private:
  // Used to perform operations on network request.
  // Can be null on interrupted download.
  std::unique_ptr<DownloadRequestHandleInterface> request_handle_;

  DISALLOW_COPY_AND_ASSIGN(DownloadJobImpl);
};

}  //  namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_IMPL_H_
