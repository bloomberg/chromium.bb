// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/drag_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "ipc/ipc_message.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#import "ui/base/dragdrop/cocoa_dnd_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using content::PluginService;

namespace drag_util {

namespace {

BOOL IsSupportedFileURL(Profile* profile, const GURL& url) {
  base::FilePath full_path;
  net::FileURLToFilePath(url, &full_path);

  std::string mime_type;
  net::GetMimeTypeFromFile(full_path, &mime_type);

  // This logic mirrors |BufferedResourceHandler::ShouldDownload()|.
  // TODO(asvitkine): Refactor this out to a common location instead of
  //                  duplicating code.
  if (net::IsSupportedMimeType(mime_type))
    return YES;

  // Check whether there is a plugin that supports the mime type. (e.g. PDF)
  // TODO(bauerb): This possibly uses stale information, but it's guaranteed not
  // to do disk access.
  bool allow_wildcard = false;
  content::WebPluginInfo plugin;
  return PluginService::GetInstance()->GetPluginInfo(
      -1,                // process ID
      MSG_ROUTING_NONE,  // routing ID
      profile->GetResourceContext(),
      url, GURL(), mime_type, allow_wildcard,
      NULL, &plugin, NULL);
}

}  // namespace

GURL GetFileURLFromDropData(id<NSDraggingInfo> info) {
  if ([[info draggingPasteboard] containsURLData]) {
    GURL url;
    ui::PopulateURLAndTitleFromPasteboard(&url,
                                          NULL,
                                          [info draggingPasteboard],
                                          YES);

    if (url.SchemeIs(url::kFileScheme))
      return url;
  }
  return GURL();
}

BOOL IsUnsupportedDropData(Profile* profile, id<NSDraggingInfo> info) {
  GURL url = GetFileURLFromDropData(info);
  if (!url.is_empty()) {
    // If dragging a file, only allow dropping supported file types (that the
    // web view can display).
    return !IsSupportedFileURL(profile, url);
  }
  return NO;
}

}  // namespace drag_util
