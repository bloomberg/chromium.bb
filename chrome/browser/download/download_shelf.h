// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
#pragma once

class BaseDownloadItemModel;
class Browser;

// This is an interface for platform specific download shelf implementations.
class DownloadShelf {
 public:
  virtual ~DownloadShelf() {}

  // A new download has started, so add it to our shelf. This object will
  // take ownership of |download_model|. Also make the shelf visible.
  virtual void AddDownload(BaseDownloadItemModel* download_model) = 0;

  // The browser view needs to know when we are going away to properly return
  // the resize corner size to WebKit so that we don't draw on top of it.
  // This returns the showing state of our animation which is set to true at
  // the beginning Show and false at the beginning of a Hide.
  virtual bool IsShowing() const = 0;

  // Returns whether the download shelf is showing the close animation.
  virtual bool IsClosing() const = 0;

  // Opens the shelf.
  virtual void Show() = 0;

  // Closes the shelf.
  virtual void Close() = 0;

  virtual Browser* browser() const = 0;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
