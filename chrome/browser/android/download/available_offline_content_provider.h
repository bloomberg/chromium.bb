// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_AVAILABLE_OFFLINE_CONTENT_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_AVAILABLE_OFFLINE_CONTENT_PROVIDER_H_

#include "chrome/common/available_offline_content.mojom.h"

namespace content {
class BrowserContext;
}

namespace android {

// Provides access to items available while offline.
class AvailableOfflineContentProvider
    : public chrome::mojom::AvailableOfflineContentProvider {
 public:
  using ListCallback = base::OnceCallback<void(
      std::vector<chrome::mojom::AvailableOfflineContentPtr>)>;

  // Public for testing.
  explicit AvailableOfflineContentProvider(
      content::BrowserContext* browser_context);
  ~AvailableOfflineContentProvider() override;

  // chrome::mojom::AvailableOfflineContentProvider methods.
  void List(ListCallback callback) override;

  static void Create(
      content::BrowserContext* browser_context,
      chrome::mojom::AvailableOfflineContentProviderRequest request);

 private:
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(AvailableOfflineContentProvider);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_AVAILABLE_OFFLINE_CONTENT_PROVIDER_H_
