// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utilities.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "gfx/native_widget_types.h"

#if defined(TOOLKIT_VIEWS)
#include "views/view.h"
#endif

namespace gfx {
class Canvas;
}

class BaseDownloadItemModel;
class DictionaryValue;
class DownloadItem;
class FilePath;
class GURL;
class ResourceDispatcherHost;
class SkBitmap;
class URLRequestContextGetter;

struct DownloadSaveInfo;

namespace download_util {

// Download opening ------------------------------------------------------------

// Whether it is OK to open this download.
bool CanOpenDownload(DownloadItem* download);

// Open the file associated with this download (wait for the download to
// complete if it is in progress).
void OpenDownload(DownloadItem* download);

// Download temporary file creation --------------------------------------------

// Return the default download directory.
const FilePath& GetDefaultDownloadDirectory();

// Create a temporary file for a download in the user's default download
// directory and return true if was successful in creating the file.
bool CreateTemporaryFileForDownload(FilePath* path);

// Return true if the |download_path| is dangerous path.
bool DownloadPathIsDangerous(const FilePath& download_path);

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

// The offset required to center the icon in the progress bitmaps.
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

// Drag support ----------------------------------------------------------------

// Helper function for download views to use when acting as a drag source for a
// DownloadItem. If |icon| is NULL, no image will be accompany the drag. |view|
// is only required for Mac OS X, elsewhere it can be NULL.
void DragDownload(const DownloadItem* download,
                  SkBitmap* icon,
                  gfx::NativeView view);

// Executable file support -----------------------------------------------------

// Determine if the specified extension is an executable extension.
bool IsExecutableExtension(const std::string& extension);

// Helpers ---------------------------------------------------------------------

// Creates a representation of a download in a format that the downloads
// HTML page can understand.
DictionaryValue* CreateDownloadItemValue(DownloadItem* download, int id);

// Get the localized status text for an in-progress download.
std::wstring GetProgressStatusText(DownloadItem* download);

// Update the application icon to indicate overall download progress.
// |download_count| is the number of downloads currently in progress. If
// |progress_known| is false, then at least one download is of indeterminate
// size and |progress| is invalid, otherwise |progress| indicates the overall
// download progress (float value from 0..1).
void UpdateAppIconDownloadProgress(int download_count,
                                   bool progress_known,
                                   float progress);

// Appends the passed the number between parenthesis the path before the
// extension.
void AppendNumberToPath(FilePath* path, int number);

// Attempts to find a number that can be appended to that path to make it
// unique. If |path| does not exist, 0 is returned.  If it fails to find such
// a number, -1 is returned.
int GetUniquePathNumber(const FilePath& path);

// Download the URL. Must be called on the IO thread.
void DownloadUrl(const GURL& url,
                 const GURL& referrer,
                 const std::string& referrer_charset,
                 const DownloadSaveInfo& save_info,
                 ResourceDispatcherHost* rdh,
                 int render_process_host_id,
                 int render_view_id,
                 URLRequestContextGetter* request_context_getter);

// Tells the resource dispatcher host to cancel a download request.
// Must be called on the IO thread.
void CancelDownloadRequest(ResourceDispatcherHost* rdh,
                           int render_process_id,
                           int request_id);

// Same as GetUniquePathNumber, except that it also checks the existence
// of its .crdownload intermediate path.
// If |path| does not exist, 0 is returned.  If it fails to find such
// a number, -1 is returned.
int GetUniquePathNumberWithCrDownload(const FilePath& path);

// Returns a .crdownload intermediate path for the |suggested_path|.
FilePath GetCrDownloadPath(const FilePath& suggested_path);

}  // namespace download_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
