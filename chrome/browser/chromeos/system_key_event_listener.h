// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_KEY_EVENT_LISTENER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_KEY_EVENT_LISTENER_H_

#include <gtk/gtk.h>

#include <string>

#include "base/singleton.h"
#include "chrome/browser/chromeos/wm_message_listener.h"

namespace chromeos {

class SystemKeyEventListener : public WmMessageListener::Observer {
 public:
  static SystemKeyEventListener* instance();

  // WmMessageListener::Observer:
  virtual void ProcessWmMessage(const WmIpc::Message& message,
                                GdkWindow* window);

 private:
  // Defines the delete on exit Singleton traits we like.  Best to have this
  // and const/dest private as recommended for Singletons.
  friend struct DefaultSingletonTraits<SystemKeyEventListener>;

  SystemKeyEventListener();
  virtual ~SystemKeyEventListener();

  void RunCommand(std::string command);

  DISALLOW_COPY_AND_ASSIGN(SystemKeyEventListener);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_KEY_EVENT_LISTENER_H_

