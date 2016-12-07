// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_NOTIFICATION_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_NOTIFICATION_BRIDGE_H_

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "components/offline_pages/core/downloads/offline_page_download_notifier.h"

namespace offline_pages {
namespace android {

// Bridge for offline page related keyed services to issue download
// notifications in Android.
class OfflinePageNotificationBridge : public OfflinePageDownloadNotifier {
 public:
  ~OfflinePageNotificationBridge() override {}

  void NotifyDownloadSuccessful(const DownloadUIItem& item) override;
  void NotifyDownloadFailed(const DownloadUIItem& item) override;
  void NotifyDownloadProgress(const DownloadUIItem& item) override;
  void NotifyDownloadPaused(const DownloadUIItem& item) override;
  void NotifyDownloadInterrupted(const DownloadUIItem& item) override;
  void NotifyDownloadCanceled(const DownloadUIItem& item) override;

  void ShowDownloadingToast();
};

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_NOTIFICATION_BRIDGE_H_
