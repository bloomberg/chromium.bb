// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BROWSER_CDM_H_
#define MEDIA_BASE_BROWSER_CDM_H_

#include "media/base/media_keys.h"
#include "media/base/player_tracker.h"

namespace media {

// Interface for browser side CDMs.
class BrowserCdm : public MediaKeys, public PlayerTracker {
 public:
  virtual ~BrowserCdm();

  // MediaKeys implementation.
  virtual bool CreateSession(uint32 session_id,
                             const std::string& content_type,
                             const uint8* init_data,
                             int init_data_length) = 0;
  virtual void LoadSession(uint32 session_id,
                           const std::string& web_session_id) = 0;
  virtual void UpdateSession(uint32 session_id,
                             const uint8* response,
                             int response_length) = 0;
  virtual void ReleaseSession(uint32 session_id) = 0;

  // PlayerTracker implementation.
  virtual int RegisterPlayer(const base::Closure& new_key_cb,
                             const base::Closure& cdm_unset_cb) = 0;
  virtual void UnregisterPlayer(int registration_id) = 0;

 protected:
   BrowserCdm();

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCdm);
};

}  // namespace media

#endif  // MEDIA_BASE_BROWSER_CDM_H_
