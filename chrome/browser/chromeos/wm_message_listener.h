// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_MESSAGE_LISTENER_H_
#define CHROME_BROWSER_CHROMEOS_WM_MESSAGE_LISTENER_H_
#pragma once

#include <gtk/gtk.h>

#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/wm_ipc.h"

class Browser;
class BrowserView;

namespace chromeos {

// WmMessageListener listens for messages from the window manager that need to
// be dealt with by Chrome.
// To listen for window manager messages add an observer.
class WmMessageListener : public MessageLoopForUI::Observer {
 public:
  // Observer is notified any time a message is received from the window
  // manager.
  class Observer {
   public:
    virtual void ProcessWmMessage(const WmIpc::Message& message,
                                  GdkWindow* window) = 0;
  };

  static WmMessageListener* GetInstance();

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // MessageLoopForUI::Observer overrides.
  virtual void WillProcessEvent(GdkEvent* event);
  virtual void DidProcessEvent(GdkEvent* event);

 private:
  friend struct DefaultSingletonTraits<WmMessageListener>;

  WmMessageListener();
  ~WmMessageListener();

  // Invoked when a valid WmIpc::Message is received.
  void ProcessMessage(const WmIpc::Message& message, GdkWindow* window);

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(WmMessageListener);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_MESSAGE_LISTENER_H_
