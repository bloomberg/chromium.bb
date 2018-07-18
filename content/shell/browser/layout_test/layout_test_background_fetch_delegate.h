// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BACKGROUND_FETCH_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BACKGROUND_FETCH_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "components/download/public/background_service/client.h"
#include "content/public/browser/background_fetch_delegate.h"

namespace download {
class DownloadService;
}  // namespace download

namespace content {

class BrowserContext;

class LayoutTestBackgroundFetchDelegate : public BackgroundFetchDelegate {
 public:
  explicit LayoutTestBackgroundFetchDelegate(BrowserContext* browser_context);
  ~LayoutTestBackgroundFetchDelegate() override;

  // BackgroundFetchDelegate implementation:
  void GetIconDisplaySize(GetIconDisplaySizeCallback callback) override;
  void CreateDownloadJob(
      std::unique_ptr<BackgroundFetchDescription> fetch_description) override;
  void DownloadUrl(const std::string& job_unique_id,
                   const std::string& download_guid,
                   const std::string& method,
                   const GURL& url,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation,
                   const net::HttpRequestHeaders& headers) override;
  void Abort(const std::string& job_unique_id) override;
  void UpdateUI(const std::string& job_unique_id,
                const std::string& title) override;

 private:
  class LayoutTestBackgroundFetchDownloadClient;

  BrowserContext* browser_context_;

  // In-memory instance of the Download Service lazily created by the delegate.
  std::unique_ptr<download::DownloadService> download_service_;

  // Weak reference to an instance of our download client.
  LayoutTestBackgroundFetchDownloadClient* background_fetch_client_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestBackgroundFetchDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BACKGROUND_FETCH_DELEGATE_H_
