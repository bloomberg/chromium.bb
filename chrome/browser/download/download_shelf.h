// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"

class Browser;

namespace gfx {
class Canvas;
class ImageSkia;
class Rect;
}

namespace content {
class DownloadItem;
class DownloadManager;
}

// This is an abstract base class for platform specific download shelf
// implementations.
class DownloadShelf {
 public:
  // Reason for closing download shelf.
  enum CloseReason {
    // Closing the shelf automatically. E.g.: all remaining downloads in the
    // shelf have been opened, last download in shelf was removed, or the
    // browser is switching to full-screen mode.
    AUTOMATIC,

    // Closing shelf due to a user selection. E.g.: the user clicked on the
    // 'close' button on the download shelf, or the shelf is being closed as a
    // side-effect of the user opening the downloads page.
    USER_ACTION
  };

  enum PaintDownloadProgressSize {
    SMALL = 0,
    BIG
  };

  // Download progress animations ----------------------------------------------

  enum {
    // Arc sweep angle for use with downloads of unknown size.
    kUnknownAngleDegrees = 50,

    // Rate of progress for use with downloads of unknown size.
    kUnknownIncrementDegrees = 12,

    // Start angle for downloads with known size (midnight position).
    kStartAngleDegrees = -90,

    // A the maximum number of degrees of a circle.
    kMaxDegrees = 360,

    // Progress animation timer period, in milliseconds.
    kProgressRateMs = 150,

    // XP and Vista must support icons of this size.
    kSmallIconSize = 16,
    kBigIconSize = 32,

    kSmallProgressIconSize = 39,
    kBigProgressIconSize = 52,

    kSmallProgressIconOffset = (kSmallProgressIconSize - kSmallIconSize) / 2
  };

  // Type of the callback used on toolkit-views platforms for the |rtl_mirror|
  // argument of the PaintDownload functions. It captures the View subclass
  // within which the progress animation is drawn and is used to update the
  // correct 'left' value for the given rectangle in RTL locales. This is used
  // to mirror the position of the progress animation. The callback is
  // guaranteed to be invoked before the paint function returns.
  typedef base::Callback<void(gfx::Rect*)> BoundsAdjusterCallback;

  DownloadShelf();
  virtual ~DownloadShelf();

  // Our progress halo around the icon.
  // Load a language dependent height so that the dangerous download
  // confirmation message doesn't overlap with the download link label.
  static int GetBigProgressIconSize();

  // The offset required to center the icon in the progress images.
  static int GetBigProgressIconOffset();

  // Paint the common download animation progress foreground and background,
  // clipping the foreground to 'percent' full. If percent is -1, then we don't
  // know the total size, so we just draw a rotating segment until we're done.
  static void PaintCustomDownloadProgress(
      gfx::Canvas* canvas,
      const gfx::ImageSkia& background_image,
      const gfx::ImageSkia& foreground_image,
      int image_size,
      const gfx::Rect& bounds,
      int start_angle,
      int percent_done);

  static void PaintDownloadProgress(gfx::Canvas* canvas,
                                    const BoundsAdjusterCallback& rtl_mirror,
                                    int origin_x,
                                    int origin_y,
                                    int start_angle,
                                    int percent,
                                    PaintDownloadProgressSize size);

  static void PaintDownloadComplete(gfx::Canvas* canvas,
                                    const BoundsAdjusterCallback& rtl_mirror,
                                    int origin_x,
                                    int origin_y,
                                    double animation_progress,
                                    PaintDownloadProgressSize size);

  static void PaintDownloadInterrupted(gfx::Canvas* canvas,
                                       const BoundsAdjusterCallback& rtl_mirror,
                                       int origin_x,
                                       int origin_y,
                                       double animation_progress,
                                       PaintDownloadProgressSize size);

  // A new download has started. Add it to our shelf and show the download
  // started animation.
  //
  // Some downloads are removed from the shelf on completion (See
  // DownloadItemModel::ShouldRemoveFromShelfWhenComplete()). These transient
  // downloads are added to the shelf after a delay. If the download completes
  // before the delay duration, it will not be added to the shelf at all.
  void AddDownload(content::DownloadItem* download);

  // The browser view needs to know when we are going away to properly return
  // the resize corner size to WebKit so that we don't draw on top of it.
  // This returns the showing state of our animation which is set to true at
  // the beginning Show and false at the beginning of a Hide.
  virtual bool IsShowing() const = 0;

  // Returns whether the download shelf is showing the close animation.
  virtual bool IsClosing() const = 0;

  // Opens the shelf.
  void Show();

  // Closes the shelf.
  void Close(CloseReason reason);

  // Hides the shelf. This closes the shelf if it is currently showing.
  void Hide();

  // Unhides the shelf. This will cause the shelf to be opened if it was open
  // when it was hidden, or was shown while it was hidden.
  void Unhide();

  virtual Browser* browser() const = 0;

  // Returns whether the download shelf is hidden.
  bool is_hidden() { return is_hidden_; }

 protected:
  virtual void DoAddDownload(content::DownloadItem* download) = 0;
  virtual void DoShow() = 0;
  virtual void DoClose(CloseReason reason) = 0;

  // Time delay to wait before adding a transient download to the shelf.
  // Protected virtual for testing.
  virtual base::TimeDelta GetTransientDownloadShowDelay();

  // Returns the DownloadManager associated with this DownloadShelf. All
  // downloads that are shown on this shelf is expected to belong to this
  // DownloadManager. Protected virtual for testing.
  virtual content::DownloadManager* GetDownloadManager();

 private:
  // Show the download on the shelf immediately. Also displayes the download
  // started animation if necessary.
  void ShowDownload(content::DownloadItem* download);

  // Similar to ShowDownload() but refers to the download using an ID. This
  // download should belong to the DownloadManager returned by
  // GetDownloadManager().
  void ShowDownloadById(int32 download_id);

  bool should_show_on_unhide_;
  bool is_hidden_;
  base::WeakPtrFactory<DownloadShelf> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
