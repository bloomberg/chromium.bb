// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utilities.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/view.h"
#endif

class GURL;

namespace base {
class DictionaryValue;
}

namespace content {
class DownloadItem;
}

namespace gfx {
class Canvas;
class Image;
}

namespace download_util {

// Download temporary file creation --------------------------------------------

// Return the default download directory.
const FilePath& GetDefaultDownloadDirectory();

// Return true if the |download_path| is dangerous path.
bool DownloadPathIsDangerous(const FilePath& download_path);

// Generate a filename based on the response from the server.  Similar
// in operation to net::GenerateFileName(), but uses a localized
// default name.
void GenerateFileNameFromRequest(const content::DownloadItem& download_item,
                                 FilePath* generated_name);

// Download progress animations ------------------------------------------------

// Arc sweep angle for use with downloads of unknown size
const int kUnknownAngleDegrees = 50;

// Rate of progress for use with downloads of unknown size
const int kUnknownIncrementDegrees = 12;

// Start angle for downloads with known size (midnight position)
const int kStartAngleDegrees = -90;

// A circle
const int kMaxDegrees = 360;

// Progress animation timer period, in milliseconds.
const int kProgressRateMs = 150;

// XP and Vista must support icons of this size.
const int kSmallIconSize = 16;
const int kBigIconSize = 32;

// Our progress halo around the icon.
int GetBigProgressIconSize();

const int kSmallProgressIconSize = 39;
const int kBigProgressIconSize = 52;

// The offset required to center the icon in the progress images.
int GetBigProgressIconOffset();

const int kSmallProgressIconOffset =
    (kSmallProgressIconSize - kSmallIconSize) / 2;

enum PaintDownloadProgressSize {
  SMALL = 0,
  BIG
};

// Paint the common download animation progress foreground and background,
// clipping the foreground to 'percent' full. If percent is -1, then we don't
// know the total size, so we just draw a rotating segment until we're done.
//
// |containing_view| is the View subclass within which the progress animation
// is drawn (generally either DownloadItemTabView or DownloadItemView). We
// require the containing View in addition to the canvas because if we are
// drawing in a right-to-left locale, we need to mirror the position of the
// progress animation within the containing View.
void PaintDownloadProgress(gfx::Canvas* canvas,
#if defined(TOOLKIT_VIEWS)
                           views::View* containing_view,
#endif
                           int origin_x,
                           int origin_y,
                           int start_angle,
                           int percent,
                           PaintDownloadProgressSize size);

void PaintDownloadComplete(gfx::Canvas* canvas,
#if defined(TOOLKIT_VIEWS)
                           views::View* containing_view,
#endif
                           int origin_x,
                           int origin_y,
                           double animation_progress,
                           PaintDownloadProgressSize size);

void PaintDownloadInterrupted(gfx::Canvas* canvas,
#if defined(TOOLKIT_VIEWS)
                              views::View* containing_view,
#endif
                              int origin_x,
                              int origin_y,
                              double animation_progress,
                              PaintDownloadProgressSize size);

// Drag support ----------------------------------------------------------------

// Helper function for download views to use when acting as a drag source for a
// DownloadItem. If |icon| is NULL, no image will be accompany the drag. |view|
// is only required for Mac OS X, elsewhere it can be NULL.
void DragDownload(const content::DownloadItem* download,
                  gfx::Image* icon,
                  gfx::NativeView view);

// Helpers ---------------------------------------------------------------------

// Creates a representation of a download in a format that the downloads
// HTML page can understand.
base::DictionaryValue* CreateDownloadItemValue(content::DownloadItem* download,
                                               int id);

// Get the localized status text for an in-progress download.
string16 GetProgressStatusText(content::DownloadItem* download);

// Update the application icon to indicate overall download progress.
// |download_count| is the number of downloads currently in progress. If
// |progress_known| is false, then at least one download is of indeterminate
// size and |progress| is invalid, otherwise |progress| indicates the overall
// download progress (float value from 0..1).
void UpdateAppIconDownloadProgress(int download_count,
                                   bool progress_known,
                                   float progress);

// Same as GetUniquePathNumber, except that it also checks the existence
// of its .crdownload intermediate path.
// If |path| does not exist, 0 is returned.  If it fails to find such
// a number, -1 is returned.
int GetUniquePathNumberWithCrDownload(const FilePath& path);

// Returns a .crdownload intermediate path for the |suggested_path|.
FilePath GetCrDownloadPath(const FilePath& suggested_path);

// Check whether we can do the saving page operation for the specified URL.
bool IsSavableURL(const GURL& url);

// Record the total number of items and the number of in-progress items showing
// in the shelf when it closes.  Set |autoclose| to true when the shelf is
// closing itself, false when the user explicitly closed it.
void RecordShelfClose(int size, int in_progress, bool autoclose);

// Used for counting UMA stats. Similar to content's
// download_stats::DownloadCountTypes but from the chrome layer.
enum ChromeDownloadCountTypes {
  // Stale enum values left around os that values passed to UMA don't
  // change.
  CHROME_DOWNLOAD_COUNT_UNUSED_0 = 0,
  CHROME_DOWNLOAD_COUNT_UNUSED_1,
  CHROME_DOWNLOAD_COUNT_UNUSED_2,
  CHROME_DOWNLOAD_COUNT_UNUSED_3,

  // A download *would* have been initiated, but it was blocked
  // by the DownloadThrottlingResourceHandler.
  BLOCKED_BY_THROTTLING,

  CHROME_DOWNLOAD_COUNT_TYPES_LAST_ENTRY
};

// Used for counting UMA stats. Similar to content's
// download_stats::DownloadInitiattionSources but from the chrome layer.
enum ChromeDownloadSource {
  // The download was initiated by navigating to a URL (e.g. by user click).
  INITIATED_BY_NAVIGATION = 0,

  // The download was initiated by invoking a context menu within a page.
  INITIATED_BY_CONTEXT_MENU,

  // The download was initiated by the WebStore installer.
  INITIATED_BY_WEBSTORE_INSTALLER,

  // The download was initiated by the ImageBurner (cros).
  INITIATED_BY_IMAGE_BURNER,

  // The download was initiated by the plugin installer.
  INITIATED_BY_PLUGIN_INSTALLER,

  CHROME_DOWNLOAD_SOURCE_LAST_ENTRY,
};

// Increment one of the above counts.
void RecordDownloadCount(ChromeDownloadCountTypes type);

// Record initiation of a download from a specific source.
void RecordDownloadSource(ChromeDownloadSource source);

}  // namespace download_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
