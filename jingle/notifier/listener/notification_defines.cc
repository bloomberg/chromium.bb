// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/notification_defines.h"

namespace notifier {

std::string Notification::ToString() const {
  return "{ channel: \"" + channel + "\", data: \"" + data + "\" }";
}

}  // namespace notifier
