// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/browser/background_fetch/background_fetch_delegate.h"
#include "content/public/browser/download_interrupt_reasons.h"

namespace content {

class BrowserContext;
class DownloadItem;

// Implementation of BackgroundFetchDelegate using the legacy DownloadManager.
class CONTENT_EXPORT BackgroundFetchDelegateImpl
    : public BackgroundFetchDelegate {
 public:
  explicit BackgroundFetchDelegateImpl(BrowserContext* browser_context);

  ~BackgroundFetchDelegateImpl() override;

  // BackgroundFetchDelegate implementation:
  void DownloadUrl(const std::string& guid,
                   const std::string& method,
                   const GURL& url,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation,
                   const net::HttpRequestHeaders& headers) override;

  base::WeakPtr<BackgroundFetchDelegateImpl> GetWeakPtr();

 private:
  void DidStartRequest(DownloadItem* download_item,
                       DownloadInterruptReason interrupt_reason);

  BrowserContext* browser_context_;

  base::WeakPtrFactory<BackgroundFetchDelegateImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDelegateImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
