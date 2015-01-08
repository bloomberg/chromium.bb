// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/browser_cdm_cast.h"

namespace chromecast {
namespace media {

BrowserCdmCast::BrowserCdmCast() {
}

BrowserCdmCast::~BrowserCdmCast() {
  player_tracker_.NotifyCdmUnset();
}

int BrowserCdmCast::RegisterPlayer(const base::Closure& new_key_cb,
                                   const base::Closure& cdm_unset_cb) {
  return player_tracker_.RegisterPlayer(new_key_cb, cdm_unset_cb);
}

void BrowserCdmCast::UnregisterPlayer(int registration_id) {
  player_tracker_.UnregisterPlayer(registration_id);
}

void BrowserCdmCast::NotifyKeyAdded() {
  player_tracker_.NotifyNewKey();
}

}  // namespace media
}  // namespace chromecast
