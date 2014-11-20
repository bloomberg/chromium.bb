// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/browser_cdm_cast.h"

#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"

namespace chromecast {
namespace media {

BrowserCdmCast::BrowserCdmCast()
    : next_registration_id_(1) {
}

BrowserCdmCast::~BrowserCdmCast() {
  // Call unset callbacks synchronously.
  for (std::map<uint32_t, base::Closure>::const_iterator it =
       cdm_unset_callbacks_.begin(); it != cdm_unset_callbacks_.end(); ++it) {
    it->second.Run();
  }

  new_key_callbacks_.clear();
  cdm_unset_callbacks_.clear();
}

int BrowserCdmCast::RegisterPlayer(const base::Closure& new_key_cb,
                                   const base::Closure& cdm_unset_cb) {
  int registration_id = next_registration_id_++;
  DCHECK(!new_key_cb.is_null());
  DCHECK(!cdm_unset_cb.is_null());
  DCHECK(!ContainsKey(new_key_callbacks_, registration_id));
  DCHECK(!ContainsKey(cdm_unset_callbacks_, registration_id));
  new_key_callbacks_[registration_id] = new_key_cb;
  cdm_unset_callbacks_[registration_id] = cdm_unset_cb;
  return registration_id;
}

void BrowserCdmCast::UnregisterPlayer(int registration_id) {
  DCHECK(ContainsKey(new_key_callbacks_, registration_id));
  DCHECK(ContainsKey(cdm_unset_callbacks_, registration_id));
  new_key_callbacks_.erase(registration_id);
  cdm_unset_callbacks_.erase(registration_id);
}

void BrowserCdmCast::NotifyKeyAdded() const {
  for (std::map<uint32_t, base::Closure>::const_iterator it =
       new_key_callbacks_.begin(); it != new_key_callbacks_.end(); ++it) {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE, it->second);
  }
}

}  // namespace media
}  // namespace chromecast
