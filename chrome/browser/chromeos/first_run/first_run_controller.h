// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"

namespace ash {
class FirstRunHelper;
}

namespace chromeos {

namespace first_run {
class Step;
}

// FirstRunController creates and manages first-run tutorial.
// Object manages its lifetime and deletes itself after completion of the
// tutorial.
class FirstRunController : public FirstRunActor::Delegate {
  typedef std::vector<linked_ptr<first_run::Step> > Steps;

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

  void RegisterSteps();
  void ShowNextStep();
  void AdvanceStep();
  first_run::Step* GetCurrentStep() const;

  // The object providing interface to UI layer. It's not directly owned by
  // FirstRunController.
  FirstRunActor* actor_;

  // Helper for manipulating and retreiving information from Shell.
  scoped_ptr<ash::FirstRunHelper> shell_helper_;

  // List of all tutorial steps.
  Steps steps_;

  // Index of step that is currently shown.
  size_t current_step_index_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_CONTROLLER_H_

