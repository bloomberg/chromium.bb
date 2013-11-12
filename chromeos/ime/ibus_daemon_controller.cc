// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/ibus_daemon_controller.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

namespace {

IBusDaemonController* g_ibus_daemon_controller = NULL;

// An implementation of IBusDaemonController without ibus-daemon interaction.
// Currently this class is used only on linux desktop.
// TODO(nona): Remove IBusDaemonControlelr this once crbug.com/171351 is fixed.
class IBusDaemonControllerDaemonlessImpl : public IBusDaemonController {
 public:
  IBusDaemonControllerDaemonlessImpl()
      : is_started_(false) {}
  virtual ~IBusDaemonControllerDaemonlessImpl() {}

  // IBusDaemonController overrides:
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool Start() OVERRIDE {
    if (is_started_)
      return false;
    // IBusBus should be initialized but it is okay to pass "dummy address" as
    // the bus address because the actual dbus implementation is stub if the
    // Chrome OS is working on Linux desktop. This path is not used in
    // production at this moment, only for Chrome OS on Linux Desktop.
    // TODO(nona): Remove InitIBusBus oncer all legacy ime is migrated to IME
    // extension API.
    DBusThreadManager::Get()->InitIBusBus("dummy address",
                                          base::Bind(&base::DoNothing));
    is_started_ = true;
    FOR_EACH_OBSERVER(Observer, observers_, OnConnected());
    return true;
  }
  virtual bool Stop() OVERRIDE {
    if (!is_started_)
      return false;
    is_started_ = false;
    FOR_EACH_OBSERVER(Observer, observers_, OnDisconnected());
    return true;
  }

 private:
  ObserverList<Observer> observers_;
  bool is_started_;
  DISALLOW_COPY_AND_ASSIGN(IBusDaemonControllerDaemonlessImpl);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IBusDaemonController

IBusDaemonController::IBusDaemonController() {
}

IBusDaemonController::~IBusDaemonController() {
}

// static
void IBusDaemonController::Initialize(
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner) {
  DCHECK(g_ibus_daemon_controller == NULL)
      << "Do not call Initialize function multiple times.";
  g_ibus_daemon_controller = new IBusDaemonControllerDaemonlessImpl();
}

// static
void IBusDaemonController::InitializeForTesting(
    IBusDaemonController* controller) {
  DCHECK(g_ibus_daemon_controller == NULL);
  DCHECK(controller);
  g_ibus_daemon_controller = controller;
}

// static
void IBusDaemonController::Shutdown() {
  delete g_ibus_daemon_controller;
  g_ibus_daemon_controller = NULL;
}

// static
IBusDaemonController* IBusDaemonController::GetInstance() {
  return g_ibus_daemon_controller;
}

}  // namespace chromeos
