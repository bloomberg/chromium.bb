// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_WEB_NOTIFICATION_CONTENTS_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_WEB_NOTIFICATION_CONTENTS_VIEW_H_

#include "ui/views/view.h"

namespace ash {

class WebNotificationTray;

namespace message_center {

// Base class for the contents of a web notification bubble.
class WebNotificationContentsView : public views::View {
 public:
  explicit WebNotificationContentsView(WebNotificationTray* tray);
  virtual ~WebNotificationContentsView();

 protected:
  WebNotificationTray* tray_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationContentsView);
};

}  // namespace message_center

}  // namespace ash

#endif // ASH_SYSTEM_NOTIFICATION_WEB_NOTIFICATION_CONTENTS_VIEW_H_
