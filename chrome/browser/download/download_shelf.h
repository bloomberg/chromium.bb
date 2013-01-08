// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_

namespace content {
class DownloadItem;
}

class Browser;

// This is an abstract base class for platform specific download shelf
// implementations.
class DownloadShelf {
 public:
  DownloadShelf();
  virtual ~DownloadShelf() {}

  // A new download has started, so add it to our shelf. Also make the shelf
  // visible.
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
  void Close();

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
  virtual void DoClose() = 0;

 private:
  bool should_show_on_unhide_;
  bool is_hidden_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
