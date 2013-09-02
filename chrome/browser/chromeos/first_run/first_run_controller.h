// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"

namespace views {
class Widget;
}

namespace chromeos {

// FirstRunController creates and manages first-run tutorial.
// Object manages its lifetime and deletes itself after completion of the
// tutorial.
class FirstRunController : public FirstRunActor::Delegate {
 public:
  FirstRunController();
  virtual ~FirstRunController();

  // Creates first-run UI and starts tutorial.
  void Start();

  // Finalizes first-run tutorial and destroys UI.
  void Stop();

 private:
  // Overriden from FirstRunActor::Delegate.
  virtual void OnActorInitialized() OVERRIDE;
  virtual void OnNextButtonClicked(const std::string& step_name) OVERRIDE;
  virtual void OnActorDestroyed() OVERRIDE;

  // Window with UI. FirstRunController closes window after tutorial completes.
  views::Widget* window_;
  // The object providing interface to UI layer. It's not directly owned by
  // FirstRunController.
  FirstRunActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_CONTROLLER_H_

