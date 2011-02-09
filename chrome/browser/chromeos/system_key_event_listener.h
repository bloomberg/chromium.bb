// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_KEY_EVENT_LISTENER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_KEY_EVENT_LISTENER_H_
#pragma once

#include <gdk/gdk.h>

#include "base/singleton.h"
#include "chrome/browser/chromeos/wm_message_listener.h"

namespace chromeos {

class AudioHandler;

// SystemKeyEventListener listens for volume related key presses from GDK, then
// tells the AudioHandler to adjust volume accordingly.  Start by just calling
// instance() to get it going.

// TODO(davej): Remove WmMessageListener::Observer once volume key handling has
// been removed from the window manager since those keys take precedence.

class SystemKeyEventListener : public WmMessageListener::Observer {
 public:
  static SystemKeyEventListener* GetInstance();

  // WmMessageListener::Observer:
  virtual void ProcessWmMessage(const WmIpc::Message& message,
                                GdkWindow* window);

  void Stop();

 private:
  // Defines the delete on exit Singleton traits we like.  Best to have this
  // and const/dest private as recommended for Singletons.
  friend struct DefaultSingletonTraits<SystemKeyEventListener>;

  SystemKeyEventListener();
  virtual ~SystemKeyEventListener();

  // This event filter intercepts events before they reach GDK, allowing us to
  // check for system level keyboard events regardless of which window has
  // focus.
  static GdkFilterReturn GdkEventFilter(GdkXEvent* gxevent,
                                        GdkEvent* gevent,
                                        gpointer data);

  // Tell X we are interested in the specified key/mask combination.
  // Capslock and Numlock are always ignored.
  void GrabKey(int32 key, uint32 mask);

  void OnVolumeMute();
  void OnVolumeDown();
  void OnVolumeUp();

  int32 key_volume_mute_;
  int32 key_volume_down_;
  int32 key_volume_up_;
  int32 key_f8_;
  int32 key_f9_;
  int32 key_f10_;

  bool stopped_;

  // AudioHandler is a Singleton class we are just caching a pointer to here,
  // and we do not own the pointer.
  AudioHandler* const audio_handler_;

  DISALLOW_COPY_AND_ASSIGN(SystemKeyEventListener);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_KEY_EVENT_LISTENER_H_

