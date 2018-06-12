// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MACHINE_LEVEL_USER_CLOUD_POLICY_REGISTER_WATCHER_H_
#define CHROME_BROWSER_POLICY_MACHINE_LEVEL_USER_CLOUD_POLICY_REGISTER_WATCHER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "chrome/browser/policy/machine_level_user_cloud_policy_controller.h"
#include "chrome/browser/ui/enterprise_startup_dialog.h"

namespace policy {

// Watches the status of machine level user cloud policy enrollment.
// Shows the blocking dialog for ongoing enrollment and failed enrollment.
class MachineLevelUserCloudPolicyRegisterWatcher
    : public MachineLevelUserCloudPolicyController::Observer {
 public:
  using DialogCreationCallback =
      base::OnceCallback<std::unique_ptr<EnterpriseStartupDialog>(
          EnterpriseStartupDialog::DialogResultCallback)>;

  explicit MachineLevelUserCloudPolicyRegisterWatcher(
      MachineLevelUserCloudPolicyController* controller);
  ~MachineLevelUserCloudPolicyRegisterWatcher() override;

  // Blocks until the machine level user cloud policy enrollment process
  // finishes. Returns the result of enrollment.
  MachineLevelUserCloudPolicyController::RegisterResult
  WaitUntilCloudPolicyEnrollmentFinished();

  void SetDialogCreationCallbackForTesting(DialogCreationCallback callback);

 private:
  // MachineLevelUserCloudPolicyController::Observer
  void OnPolicyRegisterFinished(bool succeeded) override;

  // EnterpriseStartupDialog callback.
  void OnDialogClosed(bool is_accepted, bool can_show_browser_window);

  void DisplayErrorMessage();

  MachineLevelUserCloudPolicyController* controller_;

  base::RunLoop run_loop_;
  std::unique_ptr<EnterpriseStartupDialog> dialog_;

  bool is_restart_needed_ = false;
  base::Optional<bool> register_result_;

  DialogCreationCallback dialog_creation_callback_;

  DISALLOW_COPY_AND_ASSIGN(MachineLevelUserCloudPolicyRegisterWatcher);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MACHINE_LEVEL_USER_CLOUD_POLICY_REGISTER_WATCHER_H_
