// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_STEP_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_STEP_H_

#include <string>

#include "base/basictypes.h"

namespace ash {
class FirstRunHelper;
}

namespace gfx {
class Size;
}

namespace chromeos {

class FirstRunActor;

namespace first_run {

class Step {
 public:
  Step(const std::string& name,
       ash::FirstRunHelper* shell_helper,
       FirstRunActor* actor);
  virtual ~Step();

  // Step shows its content.
  virtual void Show() = 0;

  // Called before hiding step. Default implementation removes holes from
  // background.
  virtual void OnBeforeHide();

  // Returns size of overlay window.
  gfx::Size GetOverlaySize() const;

  const std::string& name() const { return name_; }

 protected:
  ash::FirstRunHelper* shell_helper() const { return shell_helper_; }
  FirstRunActor* actor() const { return actor_; }

 private:
  std::string name_;
  ash::FirstRunHelper* shell_helper_;
  FirstRunActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(Step);
};

}  // namespace first_run
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_STEP_H_

