// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_MESSAGE_LISTENER_H_
#define CHROME_BROWSER_CHROMEOS_WM_MESSAGE_LISTENER_H_

#include <gtk/gtk.h>

#include "base/message_loop.h"
#include "base/singleton.h"
#include "chrome/browser/chromeos/wm_ipc.h"

class Browser;
class BrowserView;

namespace chromeos {

// WmMessageListener listens for messages from the window manager that need to
// be dealt with by Chrome.
class WmMessageListener : public MessageLoopForUI::Observer {
 public:
  static WmMessageListener* instance();

  // MessageLoopForUI::Observer overrides.
  virtual void WillProcessEvent(GdkEvent* event);
  virtual void DidProcessEvent(GdkEvent* event);

 private:
  friend struct DefaultSingletonTraits<WmMessageListener>;

  WmMessageListener();
  ~WmMessageListener();

  // Invoked when a valid WmIpc::Message is received.
  void ProcessMessage(const WmIpc::Message& message, GdkWindow* window);

  DISALLOW_COPY_AND_ASSIGN(WmMessageListener);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_MESSAGE_LISTENER_H_
