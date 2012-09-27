// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_WEB_NOTIFICATION_H_
#define ASH_SYSTEM_NOTIFICATION_WEB_NOTIFICATION_H_

#include <string>

#include "base/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace message_center {

struct WebNotification {
  WebNotification();
  virtual ~WebNotification();

  std::string id;
  string16 title;
  string16 message;
  string16 display_source;
  std::string extension_id;
  gfx::ImageSkia image;
  bool is_read;  // True if this has been seen in the message center
  bool shown_as_popup;  // True if this has been shown as a popup notification
};

}  // namespace message_center

}  // namespace ash

#endif // ASH_SYSTEM_NOTIFICATION_WEB_NOTIFICATION_H_
