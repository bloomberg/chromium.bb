// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_MOCK_IBUS_DAEMON_CONTROLLER_H_
#define CHROMEOS_IME_MOCK_IBUS_DAEMON_CONTROLLER_H_

#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/ime/ibus_daemon_controller.h"

namespace chromeos {

// A mock implementation of IBusDaemonController.
class CHROMEOS_EXPORT MockIBusDaemonController : public IBusDaemonController {
 public:
  MockIBusDaemonController();
  virtual ~MockIBusDaemonController();

  // IBusDaemonController overrides:
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool Start() OVERRIDE;
  virtual bool Stop() OVERRIDE;

  // Emulates connecting/disconnecting the connection with ibus-daemon.
  void EmulateConnect();
  void EmulateDisconnect();

  int start_count() { return start_count_;}
  int stop_count() { return stop_count_;}

 private:
  int start_count_;
  int stop_count_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(MockIBusDaemonController);
};

}  // namespace chromeos

#endif  // CHROMEOS_IME_MOCK_IBUS_DAEMON_CONTROLLER_H_
