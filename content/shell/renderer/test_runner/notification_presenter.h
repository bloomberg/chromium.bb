// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_NOTIFICATION_PRESENTER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_NOTIFICATION_PRESENTER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "third_party/WebKit/public/web/WebNotification.h"
#include "third_party/WebKit/public/web/WebNotificationPresenter.h"

namespace content {

class WebTestDelegate;

// A class that implements WebNotificationPresenter for the TestRunner library.
class NotificationPresenter : public blink::WebNotificationPresenter {
 public:
  NotificationPresenter();
  virtual ~NotificationPresenter();

  // Called by the TestRunner to simulate a user clicking on a notification.
  bool SimulateClick(const std::string& title);

  // Called by the TestRunner to reset the presenter to an default state.
  void Reset();

  void set_delegate(WebTestDelegate* delegate) { delegate_ = delegate; }

  // blink::WebNotificationPresenter interface
  virtual bool show(const blink::WebNotification& notification);
  virtual void cancel(const blink::WebNotification& notification);
  virtual void objectDestroyed(const blink::WebNotification& notification);
  virtual Permission checkPermission(
      const blink::WebSecurityOrigin& security_origin);
  virtual void requestPermission(
      const blink::WebSecurityOrigin& security_origin,
      blink::WebNotificationPermissionCallback* callback);

 private:
  WebTestDelegate* delegate_;

  // Map of currently active notifications.
  typedef std::map<std::string, blink::WebNotification> ActiveNotificationMap;
  ActiveNotificationMap active_notifications_;

  // Map of replacement identifiers to the titles of those notifications.
  std::map<std::string, std::string> replacements_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPresenter);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_NOTIFICATION_PRESENTER_H_
