// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_TRACKER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_TRACKER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"

namespace autofill_assistant {
class ScriptExecutorDelegate;

// The script tracker keeps track of which scripts are available, which are
// running, which have run, which are runnable whose preconditions are met.
//
// User of this class is responsible for retrieving and passing scripts to the
// tracker and letting the tracker know about changes to the DOM.
class ScriptTracker {
 public:
  // Listens to changes on the ScriptTracker state.
  class Listener {
   public:
    virtual ~Listener() = default;

    // Called when the set of runnable scripts have changed. |runnable_scripts|
    // are the new runnable scripts. Runnable scripts are ordered by priority.
    virtual void OnRunnableScriptsChanged(
        const std::vector<ScriptHandle>& runnable_scripts) = 0;
  };

  // |delegate| and |listener| should outlive this object and should not be
  // nullptr.
  ScriptTracker(ScriptExecutorDelegate* delegate,
                ScriptTracker::Listener* listener);

  ~ScriptTracker();

  // Updates the set of available |scripts| and check them.
  void SetAndCheckScripts(std::vector<std::unique_ptr<Script>> scripts);

  // Run the preconditions on the current set of scripts, and possibly update
  // the set of runnable scripts.
  void CheckScripts();

  // Runs a script and reports, when the script has ended, whether the run was
  // successful. Fails immediately if a script is already running.
  //
  // Scripts that are already executed won't be considered runnable anymore.
  // Call CheckScripts to refresh the set of runnable script after script
  // execution.
  void ExecuteScript(const std::string& path,
                     base::OnceCallback<void(bool)> callback);

  // Checks whether a script is currently running. There can be at most one
  // script running at a time.
  bool running() const { return executor_ != nullptr; }

 private:
  void OnScriptRun(base::OnceCallback<void(bool)> original_callback,
                   bool success);
  void UpdateRunnableScriptsIfNecessary();

  // Returns true if |runnable_| should be updated.
  bool RunnablesHaveChanged();
  void OnPreconditionCheck(Script* script, bool met_preconditions);
  void ClearAvailableScripts();

  ScriptExecutorDelegate* const delegate_;
  ScriptTracker::Listener* const listener_;

  // Paths and names of scripts known to be runnable.
  //
  // Note that the set of runnable scripts can survive a SetScripts; as
  // long as the new set of runnable script has the same path, it won't be seen
  // as a change to the set of runnable, even if the pointers have changed.
  std::vector<ScriptHandle> runnable_scripts_;

  // Sets of available scripts. SetScripts resets this and interrupts
  // any pending check.
  std::map<Script*, std::unique_ptr<Script>> available_scripts_;

  // Set of scripts that have been executed. They'll be excluded from runnable.
  // TODO(crbug.com/806868): Track script execution status and forward
  // that information to the script precondition.
  std::set<std::string> executed_scripts_;

  // Number of precondition checks run for CheckScripts that are still
  // pending.
  int pending_precondition_check_count_;

  // Scripts found to be runnable so far, in the current run of CheckScripts.
  std::vector<Script*> pending_runnable_scripts_;

  // If a script is currently running, this is the script's executor. Otherwise,
  // this is nullptr.
  std::unique_ptr<ScriptExecutor> executor_;

  base::WeakPtrFactory<ScriptTracker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScriptTracker);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_TRACKER_H_
