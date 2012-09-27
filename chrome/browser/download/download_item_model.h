// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"

class SavePackage;

namespace content {
class DownloadItem;
}

namespace gfx {
class Font;
}

// This class is an abstraction for common UI tasks associated with a download.
class BaseDownloadItemModel {
 public:
  explicit BaseDownloadItemModel(content::DownloadItem* download)
      : download_(download) { }
  virtual ~BaseDownloadItemModel() { }

  // Cancel the task corresponding to the item.
  virtual void CancelTask() = 0;

  // Returns a short one-line status string for the download.
  virtual string16 GetStatusText() const = 0;

  // Returns a string suitable for use as a tooltip. For a regular download, the
  // tooltip is the filename. For an interrupted download, the string states the
  // filename and a short description of the reason for interruption. For
  // example:
  //    Report.pdf
  //    Network disconnected
  // |font| and |max_width| are used to elide the filename and/or interrupt
  // reason as necessary to keep the width of the tooltip text under
  // |max_width|. The tooltip will be at most 2 lines.
  virtual string16 GetTooltipText(const gfx::Font& font,
                                  int max_width) const = 0;

  // Rough percent complete. Returns -1 if the progress is unknown.
  virtual int PercentComplete() const = 0;

  // Get the warning text to display for a dangerous download. The |base_width|
  // is the maximum width of an embedded filename (if there is one). The metrics
  // for the filename will be based on |font|. Should only be called if
  // IsDangerous() is true.
  virtual string16 GetWarningText(const gfx::Font& font,
                                  int base_width) const = 0;

  // Get the caption text for a button for confirming a dangerous download
  // warning.
  virtual string16 GetWarningConfirmButtonText() const = 0;

  // Is this considered a malicious download? Implies IsDangerous().
  virtual bool IsMalicious() const = 0;

  // Is this considered a dangerous download?
  virtual bool IsDangerous() const = 0;

  // Get the total number of bytes for this download. Should return 0 if the
  // total size of the download is not known.
  virtual int64 GetTotalBytes() const = 0;

  // Get the number of bytes that has completed so far.
  virtual int64 GetCompletedBytes() const = 0;

  content::DownloadItem* download() { return download_; }

  // Get the status message of the given interrupt |reason|.
  static string16 InterruptReasonStatusMessage(int reason);

  // Get the description of the given interrupt |reason|.
  static string16 InterruptReasonMessage(int reason);

 protected:
  content::DownloadItem* download_;
};

// Concrete implementation of BaseDownloadItemModel.
class DownloadItemModel : public BaseDownloadItemModel {
 public:
  explicit DownloadItemModel(content::DownloadItem* download);
  virtual ~DownloadItemModel() { }

  // BaseDownloadItemModel
  virtual void CancelTask() OVERRIDE;
  virtual string16 GetStatusText() const OVERRIDE;
  virtual string16 GetTooltipText(const gfx::Font& font,
                                  int max_width) const OVERRIDE;
  virtual int PercentComplete() const OVERRIDE;
  virtual string16 GetWarningText(const gfx::Font& font,
                                  int base_width) const OVERRIDE;
  virtual string16 GetWarningConfirmButtonText() const OVERRIDE;
  virtual bool IsMalicious() const OVERRIDE;
  virtual bool IsDangerous() const OVERRIDE;
  virtual int64 GetTotalBytes() const OVERRIDE;
  virtual int64 GetCompletedBytes() const OVERRIDE;

 protected:
  // Returns true if |download_| is a Drive dwonload. Protected virtual for
  // testing.
  virtual bool IsDriveDownload() const;

 private:
  // Returns a string representations of the current download progress sizes. If
  // the total size of the download is known, this string looks like: "100/200
  // MB" where the numerator is the transferred size and the denominator is the
  // total size. If the total isn't known, returns the transferred size as a
  // string (e.g.: "100 MB").
  string16 GetProgressSizesString() const;

  // Returns a string indicating the status of an in-progress download.
  string16 GetInProgressStatusString() const;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemModel);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_
