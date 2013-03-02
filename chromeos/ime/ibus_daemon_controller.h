// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_IBUS_DAEMON_CONTROLLER_H_
#define CHROMEOS_IME_IBUS_DAEMON_CONTROLLER_H_

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// IBusDaemonController is used to start/stop ibus-daemon process. This class is
// also watching ibus-daemon process and if it is crashed, re-launch it
// automatically. This class is managed as Singleton.
class CHROMEOS_EXPORT IBusDaemonController {
 public:
  class Observer {
   public:
    // Called when the connection with ibus-daemon is established.
    virtual void OnConnected() = 0;

    // Called when the connection with ibus-daemon is lost.
    virtual void OnDisconnected() = 0;
  };

  // Initializes IBusDaemonController. ibus-daemon related file handling is
  // posted to |file_task_runner|. This function must be called before any
  // GetInstance() function call.
  static void Initialize(
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& file_task_runner);

  // Similar to Initialize(), but can inject alternative IBusDaemonController
  // such as MockIBusDaemonController for testing. The injected object object
  // will be owned by the internal pointer and deleted by Shutdown().
  static void InitializeForTesting(IBusDaemonController* controller_);

  // Destroys the global instance.
  static void Shutdown();

  // Returns actual implementation of IBusDaemonController. If the browser is
  // not running on the Chrome OS device, this function returns stub
  // implementation.
  static CHROMEOS_EXPORT IBusDaemonController* GetInstance();

  virtual ~IBusDaemonController();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Starts ibus-daemon.
  virtual bool Start() = 0;

  // Stops ibus-daemon.
  virtual bool Stop() = 0;

 protected:
  // IBusDaemonController is managed as singleton. USe GetInstance instead.
  IBusDaemonController();

  DISALLOW_COPY_AND_ASSIGN(IBusDaemonController);
};

}  // namespace chromeos

#endif  // CHROMEOS_IME_IBUS_DAEMON_CONTROLLER_H_
