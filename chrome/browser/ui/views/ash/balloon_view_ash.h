// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_BALLOON_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_BALLOON_VIEW_ASH_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/balloon.h"

class GURL;
class SkBitmap;

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

 private:
  // Favicon download callback.
  void DidDownloadFavicon(
      int id,
      const GURL& image_url,
      bool errored,
      int requested_size,
      const std::vector<SkBitmap>& bitmaps);
  void FetchIcon(const Notification& notification);
  std::string GetExtensionId(Balloon* balloon);

  BalloonCollection* collection_;
  Balloon* balloon_;

  // Track the current notification id and download id so that it can be updated
  // properly.
  int current_download_id_;
  std::string current_notification_id_;
  std::string cached_notification_id_;
  base::WeakPtrFactory<BalloonViewAsh> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_BALLOON_VIEW_ASH_H_
