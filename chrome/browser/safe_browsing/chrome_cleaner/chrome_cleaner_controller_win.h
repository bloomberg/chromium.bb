// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_WIN_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"

namespace safe_browsing {

// Controller class that keeps track of the execution of the Chrome Cleaner and
// the various states through which the execution will transition. Observers can
// register themselves to be notified of state changes. Intended to be used by
// the Chrome Cleaner webui page and the Chrome Cleaner prompt dialog.
//
// This class lives on, and all its members should be called only on, the UI
// thread.
class ChromeCleanerController
    : public base::RefCounted<ChromeCleanerController> {
 public:
  enum class State {
    // The default state where there is no Chrome Cleaner process or IPC to keep
    // track of and a reboot of the machine is not required to complete previous
    // cleaning attempts. This is also the state the controller will end up in
    // if any errors occur during execution of the Chrome Cleaner process.
    kIdle,
    // All steps up to and including scanning the machine occur in this
    // state. The steps include downloading the Chrome Cleaner binary, setting
    // up an IPC between Chrome and the Cleaner process, and the actual
    // scanning done by the Cleaner.
    kScanning,
    // Scanning has been completed and harmful or unwanted software was found.
    kInfected,
    // The Cleaner process is cleaning the machine.
    kCleaning,
    // The cleaning has finished, but a reboot of the machine is required to
    // complete cleanup. This state will persist across restarts of Chrome until
    // a reboot is detected.
    kRebootRequired,
  };

  class Observer {
   public:
    virtual void OnIdle() {}
    virtual void OnScanning() {}
    virtual void OnInfected() {}
    virtual void OnCleaning() {}
    virtual void OnRebootRequired() {}

   protected:
    virtual ~Observer() = default;
  };

  // Returns an existing instance of the controller if there is one, or else
  // will create a new instance. There is at most one instance of the controller
  // class at any time.
  static scoped_refptr<ChromeCleanerController> GetInstance();

  State state() const { return state_; }

  // |AddObserver()| immediately notifies |observer| of the controller's state
  // by calling the corresponding |On*()| function.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // TODO(alito): add other functions, such as Scan(), Clean() etc.

  static ChromeCleanerController* GetRawInstanceForTesting();

 private:
  friend class base::RefCounted<ChromeCleanerController>;

  ChromeCleanerController();
  ~ChromeCleanerController();

  void NotifyObserver(Observer* observer) const;
  void NotifyAllObservers() const;
  void SetKeepAlive(bool keep_alive);

  State state_ = State::kIdle;

  base::ObserverList<Observer> observer_list_;

  // Used to indicate that this instance needs to be kept alive. The instance
  // should not be destroyed while in any of the |kScanning|, |kInfected|, or
  // |kCleaning| states, which are the states where we have an active IPC to the
  // Chrome Cleaner process.
  bool keep_alive_ = false;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerController);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_WIN_H_
