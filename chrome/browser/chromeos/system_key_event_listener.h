// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_KEY_EVENT_LISTENER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_KEY_EVENT_LISTENER_H_

#include "base/singleton.h"
#include "chrome/browser/chromeos/wm_message_listener.h"

namespace chromeos {

class AudioHandler;

// SystemKeyEventListener listens for volume related key presses from the
// window manager, then tells the AudioHandler to adjust volume accordingly.
// Start by just calling instance() to get it going.

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

  // AudioHandler is a Singleton class we are just caching a pointer to here,
  // and we do not own the pointer.
  AudioHandler* const audio_handler_;

  DISALLOW_COPY_AND_ASSIGN(SystemKeyEventListener);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_KEY_EVENT_LISTENER_H_

