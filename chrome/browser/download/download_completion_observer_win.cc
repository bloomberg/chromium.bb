// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_completion_observer_win.h"

#include <string>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/win/metro.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::DownloadItem;
using content::DownloadManager;

static const char kDownloadNotificationPrefix[] = "DownloadNotification";
int g_next_notification_id = 0;

DownloadCompletionObserver::DownloadCompletionObserver(
    DownloadManager* manager) {
  manager->AddObserver(this);
}

DownloadCompletionObserver::~DownloadCompletionObserver() {
  DCHECK(download_items_.empty());
}

void DownloadCompletionObserver::OnDownloadCreated(DownloadManager* manager,
                                                   DownloadItem* download) {
  if (download->IsInProgress()) {
    download_items_.insert(download);
    download->AddObserver(this);
  }
}

void DownloadCompletionObserver::ManagerGoingDown(DownloadManager* manager) {
  ClearDownloadItems();
  manager->RemoveObserver(this);
  delete this;
}

void DownloadCompletionObserver::OnDownloadUpdated(DownloadItem* download) {
  switch (download->GetState()) {
    case DownloadItem::COMPLETE: {
      if (base::win::IsMetroProcess() &&
          !download->GetOpenWhenComplete() &&
          !download->ShouldOpenFileBasedOnExtension() &&
          !download->IsTemporary() &&
          !download->GetAutoOpened()) {
        // In Windows 8 metro mode display a metro style notification which
        // informs the user that the download is complete.
        HMODULE metro = base::win::GetMetroModule();
        base::win::MetroNotification display_notification =
            reinterpret_cast< base::win::MetroNotification>(
                ::GetProcAddress(metro, "DisplayNotification"));
        DCHECK(display_notification);
        if (display_notification) {
          string16 title = l10n_util::GetStringUTF16(
              IDS_METRO_DOWNLOAD_COMPLETE_NOTIFICATION_TITLE);
          string16 body = l10n_util::GetStringUTF16(
              IDS_METRO_DOWNLOAD_COMPLETE_NOTIFICATION);

          // Dummy notification id. Every metro style notification needs a
          // unique notification id.
          std::string notification_id = kDownloadNotificationPrefix;
          notification_id += base::IntToString(g_next_notification_id++);

          display_notification(download->GetURL().spec().c_str(),
                               "",
                               title.c_str(),
                               body.c_str(),
                               L"",
                               notification_id.c_str());
        }
      }
      DCHECK(ContainsKey(download_items_, download));
      download_items_.erase(download);
      download->RemoveObserver(this);
      break;
    }

    case DownloadItem::INTERRUPTED:
    case DownloadItem::CANCELLED: {
      DCHECK(ContainsKey(download_items_, download));
      download_items_.erase(download);
      download->RemoveObserver(this);
      break;
    }

    default:
      break;
  }
}

void DownloadCompletionObserver::OnDownloadDestroyed(DownloadItem* download) {
  DCHECK(ContainsKey(download_items_, download));
  download_items_.erase(download);
  download->RemoveObserver(this);
}

void DownloadCompletionObserver::ClearDownloadItems() {
  for (std::set<DownloadItem*>::iterator it = download_items_.begin();
       it != download_items_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  download_items_.clear();
}
