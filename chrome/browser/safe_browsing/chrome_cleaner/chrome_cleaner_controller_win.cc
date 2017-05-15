// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

// Global instance that is set and unset by ChromeCleanerController's
// constructor and destructor.
ChromeCleanerController* g_chrome_cleaner_controller = nullptr;

}  // namespace

// static
scoped_refptr<ChromeCleanerController> ChromeCleanerController::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (g_chrome_cleaner_controller)
    return make_scoped_refptr(g_chrome_cleaner_controller);

  return make_scoped_refptr(new ChromeCleanerController());
}

ChromeCleanerController* ChromeCleanerController::GetRawInstanceForTesting() {
  return g_chrome_cleaner_controller;
}

void ChromeCleanerController::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(alito): Implement this. Add to |observer_list_| and notify |observer|
  //              of the current state.
}

void ChromeCleanerController::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(alito): Implement this.
}

ChromeCleanerController::ChromeCleanerController() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!g_chrome_cleaner_controller);

  g_chrome_cleaner_controller = this;
}

ChromeCleanerController::~ChromeCleanerController() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // An instance of ChromeCleanerController must keep itself alive while in any
  // state other than |kIdle| and |kRebootRequired|.
  DCHECK(state_ == State::kIdle || state_ == State::kRebootRequired);
  DCHECK_EQ(this, g_chrome_cleaner_controller);

  g_chrome_cleaner_controller = nullptr;
}

void ChromeCleanerController::NotifyObserver(Observer* observer) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(alito): Implement this. Call |observer|'s function corresponding to
  //              the current state.
}

void ChromeCleanerController::NotifyAllObservers() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observer_list_)
    NotifyObserver(&observer);
}

void ChromeCleanerController::SetKeepAlive(bool keep_alive) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (keep_alive == keep_alive_)
    return;

  keep_alive_ = keep_alive;
  if (keep_alive_)
    AddRef();
  else
    Release();
}

}  // namespace safe_browsing
