// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_stats.h"

#include "base/metrics/histogram_macros.h"

void RecordDownloadShelfClose(int size, int in_progress, bool autoclose) {
  static const int kMaxShelfSize = 16;
  if (autoclose) {
    UMA_HISTOGRAM_ENUMERATION(
        "Download.ShelfSizeOnAutoClose", size, kMaxShelfSize);
    UMA_HISTOGRAM_ENUMERATION(
        "Download.ShelfInProgressSizeOnAutoClose", in_progress, kMaxShelfSize);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Download.ShelfSizeOnUserClose", size, kMaxShelfSize);
    UMA_HISTOGRAM_ENUMERATION(
        "Download.ShelfInProgressSizeOnUserClose", in_progress, kMaxShelfSize);
  }
}

void RecordDownloadCount(ChromeDownloadCountTypes type) {
  UMA_HISTOGRAM_ENUMERATION(
      "Download.CountsChrome", type, CHROME_DOWNLOAD_COUNT_TYPES_LAST_ENTRY);
}

void RecordDownloadSource(ChromeDownloadSource source) {
  UMA_HISTOGRAM_ENUMERATION(
      "Download.SourcesChrome", source, CHROME_DOWNLOAD_SOURCE_LAST_ENTRY);
}

void RecordDangerousDownloadWarningShown(
    content::DownloadDangerType danger_type) {
  UMA_HISTOGRAM_ENUMERATION("Download.DownloadWarningShown",
                            danger_type,
                            content::DOWNLOAD_DANGER_TYPE_MAX);
}

void RecordOpenedDangerousConfirmDialog(
    content::DownloadDangerType danger_type) {
  UMA_HISTOGRAM_ENUMERATION("Download.ShowDangerousDownloadConfirmationPrompt",
                            danger_type,
                            content::DOWNLOAD_DANGER_TYPE_MAX);
}

void RecordDownloadOpenMethod(ChromeDownloadOpenMethod open_method) {
  UMA_HISTOGRAM_ENUMERATION("Download.OpenMethod",
                            open_method,
                            DOWNLOAD_OPEN_METHOD_LAST_ENTRY);
}

void RecordDatabaseAvailability(bool is_available) {
  UMA_HISTOGRAM_BOOLEAN("Download.Database.IsAvailable", is_available);
}

void RecordDownloadPathGeneration(DownloadPathGenerationEvent event,
                                  bool is_transient) {
  if (is_transient) {
    UMA_HISTOGRAM_ENUMERATION("Download.PathGenerationEvent.Transient", event,
                              DownloadPathGenerationEvent::COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Download.PathGenerationEvent.UserDownload",
                              event, DownloadPathGenerationEvent::COUNT);
  }
}

void RecordDownloadPathValidation(PathValidationResult result,
                                  bool is_transient) {
  if (is_transient) {
    UMA_HISTOGRAM_ENUMERATION("Download.PathValidationResult.Transient", result,
                              PathValidationResult::COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Download.PathValidationResult.UserDownload",
                              result, PathValidationResult::COUNT);
  }
}

void RecordDownloadShelfDragEvent(DownloadShelfDragEvent drag_event) {
  UMA_HISTOGRAM_ENUMERATION("Download.Shelf.DragEvent", drag_event,
                            DownloadShelfDragEvent::COUNT);
}
