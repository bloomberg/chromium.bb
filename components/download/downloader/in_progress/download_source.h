// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DOWNLOADER_IN_PROGRESS_DOWNLOAD_SOURCE_H_
#define COMPONENTS_DOWNLOAD_DOWNLOADER_IN_PROGRESS_DOWNLOAD_SOURCE_H_

#include <string>

namespace download {
// The source of download.
// This enum should match content::DownloadSource in
// content/public/browser/download_source.h.
// Any changes here should also apply to download_source.proto.
enum class DownloadSource {
  UNKNOWN = 0,
  NAVIGATION = 1,
  DRAG_AND_DROP = 2,
  MANUAL_RESUMPTION = 3,
  AUTO_RESUMPTION = 4,
  FROM_RENDERER = 5,
  EXTENSION_API = 6,
  EXTENSION_INSTALLER = 7,
  PLUGIN = 8,
  PLUGIN_INSTALLER = 9,
  INTERNAL_API = 10,
  SAVE_PACKAGE = 11,
  OFFLINE_PAGE = 12,
  COUNT = 13
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DOWNLOADER_IN_PROGRESS_DOWNLOAD_SOURCE_H_
