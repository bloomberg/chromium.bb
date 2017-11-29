// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_SOURCE_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_SOURCE_H_

namespace content {

// The source of download.
// Used in UMA metrics and persisted to disk.
// Entries in this enum can only be appended instead of being deleted or reused.
// Any changes here also needs to apply to enums.xml.
enum class DownloadSource {
  // The source is unknown.
  UNKNOWN = 0,

  // Download is triggered from navigation request.
  NAVIGATION = 1,

  // Drag and drop.
  DRAG_AND_DROP = 2,

  // User manually resume the download.
  MANUAL_RESUMPTION = 3,

  // Auto resumption in download system.
  AUTO_RESUMPTION = 4,

  // Renderer initiated download, mostly from Javascript or HTML <a> tag.
  FROM_RENDERER = 5,

  // Extension download API.
  EXTENSION_API = 6,

  // Extension web store installer.
  EXTENSION_INSTALLER = 7,

  // Plugin triggered download.
  PLUGIN = 8,

  // Plugin installer download.
  PLUGIN_INSTALLER = 9,

  // Download service API background download.
  INTERNAL_API = 10,

  // Save package download.
  SAVE_PACKAGE = 11,

  // Offline page download.
  OFFLINE_PAGE = 12,

  COUNT = 13
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_SOURCE_H_
