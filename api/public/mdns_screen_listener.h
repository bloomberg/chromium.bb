// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_MDNS_SCREEN_LISTENER_H_
#define API_PUBLIC_MDNS_SCREEN_LISTENER_H_

#include "api/public/screen_listener.h"

namespace openscreen {

struct MdnsScreenListenerConfig {
  // TODO(mfoltz): Populate with actual parameters as implementation progresses.
  bool dummy_value = true;
};

class MdnsScreenListener : public ScreenListener {
 public:
  MdnsScreenListener(Observer* observer, MdnsScreenListenerConfig config);
  virtual ~MdnsScreenListener();

  // TODO: Subclass/implement remaining API

 private:
  const MdnsScreenListenerConfig config_;
};

}  // namespace openscreen

#endif  // API_PUBLIC_MDNS_SCREEN_LISTENER_H_
