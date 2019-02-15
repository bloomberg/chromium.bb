// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_MODULE_INSPECTOR_WIN_H_
#define CHROME_BROWSER_CONFLICTS_MODULE_INSPECTOR_WIN_H_

#include <map>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "chrome/browser/conflicts/inspection_results_cache_win.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"
#include "chrome/browser/conflicts/module_info_win.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"

namespace base {
class SequencedTaskRunner;
}

// This class takes care of inspecting several modules (identified by their
// ModuleInfoKey) and returning the result via the OnModuleInspectedCallback on
// the SequencedTaskRunner where it was created.
//
// The inspection of all modules is quite expensive in terms of resources, so it
// is done one by one, in a task with a background priority level. If needed, it
// is possible to increase the priority level of these tasks by calling
// IncreaseInspectionPriority().
//
// This class is not thread safe and it enforces safety via a SEQUENCE_CHECKER.
class ModuleInspector : public ModuleDatabaseObserver {
 public:
  // Temporary feature to control whether or not modules are inspected in a
  // background sequence. It will be used to assess the impact of this work on
  // Chrome's overall performance.
  // TODO(pmonette): Remove when no longer needed. See https://crbug.com/928846.
  static constexpr base::Feature kEnableBackgroundModuleInspection = {
      "EnableBackgroundModuleInspection", base::FEATURE_ENABLED_BY_DEFAULT};

  using OnModuleInspectedCallback =
      base::Callback<void(const ModuleInfoKey& module_key,
                          ModuleInspectionResult inspection_result)>;

  explicit ModuleInspector(
      const OnModuleInspectedCallback& on_module_inspected_callback);
  ~ModuleInspector() override;

  // Adds the module to the queue of modules to inspect. Starts the inspection
  // process if the |queue_| is empty.
  void AddModule(const ModuleInfoKey& module_key);

  // Removes the throttling.
  void IncreaseInspectionPriority();

  // Returns true if ModuleInspector is not doing anything right now.
  bool IsIdle();

  // ModuleDatabaseObserver:
  void OnModuleDatabaseIdle() override;

 private:
  // Invoked when Chrome has finished starting up to initiate the inspection of
  // queued modules.
  void OnStartupFinished();

  // Invoked when the InspectionResultsCache is available.
  void OnInspectionResultsCacheRead(
      InspectionResultsCache inspection_results_cache);

  // Handles a connection error to the UtilWin service.
  void OnUtilWinServiceConnectionError();

  // Starts inspecting the module at the front of the queue.
  void StartInspectingModule();

  // Adds the newly inspected module to the cache then calls
  // OnInspectionFinished().
  void OnModuleNewlyInspected(const ModuleInfoKey& module_key,
                              ModuleInspectionResult inspection_result);

  // Called back on the execution context on which the ModuleInspector was
  // created when a module has finished being inspected. The callback will be
  // executed and, if the |queue_| is not empty, the next module will be sent
  // for inspection.
  void OnInspectionFinished(const ModuleInfoKey& module_key,
                            ModuleInspectionResult inspection_result);

  OnModuleInspectedCallback on_module_inspected_callback_;

  // The modules are put in queue until they are sent for inspection.
  base::queue<ModuleInfoKey> queue_;

  // Indicates if Chrome has finished starting up. Used to delay the background
  // inspection tasks in order to not negatively impact startup performance.
  bool is_after_startup_;

  // A pointer to the UtilWin service. Only used if the WinOOPInspectModule
  // feature is enabled. It is created when inspection is ongoing, and freed
  // when no longer needed.
  chrome::mojom::UtilWinPtr util_win_ptr_;

  // The task runner where module inspections takes place. It originally starts
  // at BEST_EFFORT priority, but is changed to USER_VISIBLE when
  // IncreaseInspectionPriority() is called.
  scoped_refptr<base::SequencedTaskRunner> inspection_task_runner_;

  // The vector of paths to %env_var%, used to account for differences in
  // localization and where people keep their files.
  // e.g. c:\windows vs d:\windows
  StringMapping path_mapping_;

  // This task runner handles updates to the inspection results cache.
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;

  // Indicates if the inspection results cache was read from disk.
  bool inspection_results_cache_read_;

  // Contains the cached inspection results so that a module is not inspected
  // more than once between restarts.
  InspectionResultsCache inspection_results_cache_;

  // The number of time this class will try to restart the UtilWin service if a
  // connection error occurs. This is to prevent the degenerate case where the
  // service always fails to start and the restart cycle happens infinitely.
  int connection_error_retry_count_;

  // Indicates if background inspection is enabled. Generally equal to the
  // kBackgroundModuleInspection feature state, but will be set unconditionally
  // to true if IncreaseInspectionPriority() is called.
  bool background_inspection_enabled_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointers are used to safely post the inspection result back to the
  // ModuleInspector from the task scheduler.
  base::WeakPtrFactory<ModuleInspector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModuleInspector);
};

#endif  // CHROME_BROWSER_CONFLICTS_MODULE_INSPECTOR_WIN_H_
