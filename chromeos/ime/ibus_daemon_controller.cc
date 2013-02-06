// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/ibus_daemon_controller.h"

#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"

namespace chromeos {

namespace {

IBusDaemonController* g_ibus_daemon_controller = NULL;

// The implementation of IBusDaemonController.
class IBusDaemonControllerImpl : public IBusDaemonController {
 public:
  IBusDaemonControllerImpl() {}

  virtual ~IBusDaemonControllerImpl() {}

  // IBusDaemonController override:
  virtual void Initialize(
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& file_task_runner)
      OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    // TODO(nona): Implement this.
  }

  // IBusDaemonController override:
  virtual void AddObserver(Observer* observer) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    // TODO(nona): Implement this.
  }

  // IBusDaemonController override:
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    // TODO(nona): Implement this.
  }

  // IBusDaemonController override:
  virtual bool Start() OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    // TODO(nona): Implement this.
    return true;
  }

  // IBusDaemonController override:
  virtual bool Stop() OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    // TODO(nona): Implement this.
    return true;
  }

 private:
  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(IBusDaemonControllerImpl);
};

// The stub implementation of IBusDaemonController.
class IBusDaemonControllerStubImpl : public IBusDaemonController {
 public:
  IBusDaemonControllerStubImpl() {}
  virtual ~IBusDaemonControllerStubImpl() {}

  // IBusDaemonController overrides:
  virtual void Initialize(
      const scoped_refptr<base::SequencedTaskRunner>& ui_runner,
      const scoped_refptr<base::SequencedTaskRunner>& file_runner) OVERRIDE {}
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual bool Start() OVERRIDE { return true; }
  virtual bool Stop() OVERRIDE { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusDaemonControllerStubImpl);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IBusDaemonController

IBusDaemonController::IBusDaemonController() {
}

IBusDaemonController::~IBusDaemonController() {
}

IBusDaemonController* IBusDaemonController::GetInstance() {
  if (!g_ibus_daemon_controller) {
    if (base::chromeos::IsRunningOnChromeOS()) {
      g_ibus_daemon_controller = new IBusDaemonControllerImpl();
    } else {
      g_ibus_daemon_controller = new IBusDaemonControllerStubImpl();
    }
  }
  return g_ibus_daemon_controller;
}

}  // namespace chromeos
