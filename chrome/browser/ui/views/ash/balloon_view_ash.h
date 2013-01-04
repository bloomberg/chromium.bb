// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_BALLOON_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_BALLOON_VIEW_ASH_H_

#include <vector>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/notifications/balloon.h"

class GURL;

namespace gfx {
class ImageSkia;
}

// On Ash, a "BalloonView" is just a wrapper for ash notification entries.
class BalloonViewAsh : public BalloonView {
 public:
  explicit BalloonViewAsh(BalloonCollection* collection);
  virtual ~BalloonViewAsh();

  // BalloonView interface.
  virtual void Show(Balloon* balloon) OVERRIDE;
  virtual void Update() OVERRIDE;
  virtual void RepositionToBalloon() OVERRIDE;
  virtual void Close(bool by_user) OVERRIDE;
  virtual gfx::Size GetSize() const OVERRIDE;
  virtual BalloonHost* GetHost() const OVERRIDE;

  void SetNotificationIcon(const std::string& id, const gfx::ImageSkia& image);

 private:
  class ImageDownload;

  typedef std::vector<linked_ptr<ImageDownload> > ImageDownloads;

  void DownloadImages(const Notification& notification);

  BalloonCollection* collection_;
  Balloon* balloon_;

  // Track the current notification id and downloads so the notification can be
  // updated properly.
  std::string notification_id_;
  ImageDownloads downloads_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_BALLOON_VIEW_ASH_H_
