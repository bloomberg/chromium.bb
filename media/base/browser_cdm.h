// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BROWSER_CDM_H_
#define MEDIA_BASE_BROWSER_CDM_H_

#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_keys.h"
#include "media/base/player_tracker.h"

namespace media {

// Interface for browser side CDMs.
class MEDIA_EXPORT BrowserCdm : public MediaKeys, public PlayerTracker {
 public:
  ~BrowserCdm() override;

  // Virtual destructor. For most subclasses we can delete on the caller thread.
  virtual void DeleteOnCorrectThread();

 protected:
   BrowserCdm();

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCdm);
};

struct MEDIA_EXPORT BrowserCdmDeleter {
  inline void operator()(BrowserCdm* ptr) const {
    ptr->DeleteOnCorrectThread();
  }
};

using ScopedBrowserCdmPtr = scoped_ptr<BrowserCdm, BrowserCdmDeleter>;

}  // namespace media

#endif  // MEDIA_BASE_BROWSER_CDM_H_
