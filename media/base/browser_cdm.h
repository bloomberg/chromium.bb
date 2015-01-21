// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BROWSER_CDM_H_
#define MEDIA_BASE_BROWSER_CDM_H_

#include "media/base/media_export.h"
#include "media/base/media_keys.h"
#include "media/base/player_tracker.h"

namespace media {

// Interface for browser side CDMs.
class MEDIA_EXPORT BrowserCdm : public MediaKeys, public PlayerTracker {
 public:
  ~BrowserCdm() override;

 protected:
   BrowserCdm();

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCdm);
};

}  // namespace media

#endif  // MEDIA_BASE_BROWSER_CDM_H_
