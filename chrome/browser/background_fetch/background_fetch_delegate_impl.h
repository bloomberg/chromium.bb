// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
#define CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/download_interrupt_reasons.h"

namespace content {
class DownloadItem;
}  // namespace content

class Profile;

// Implementation of BackgroundFetchDelegate using the legacy DownloadManager.
class BackgroundFetchDelegateImpl : public content::BackgroundFetchDelegate,
                                    public KeyedService {
 public:
  explicit BackgroundFetchDelegateImpl(Profile* profile);

  ~BackgroundFetchDelegateImpl() override;

  // KeyedService implementation:
  void Shutdown() override;

  // BackgroundFetchDelegate implementation:
  void DownloadUrl(const std::string& guid,
                   const std::string& method,
                   const GURL& url,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation,
                   const net::HttpRequestHeaders& headers) override;

  base::WeakPtr<BackgroundFetchDelegateImpl> GetWeakPtr();

 private:
  void DidStartRequest(content::DownloadItem* download_item,
                       content::DownloadInterruptReason interrupt_reason);

  Profile* profile_;

  base::WeakPtrFactory<BackgroundFetchDelegateImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDelegateImpl);
};

#endif  // CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
