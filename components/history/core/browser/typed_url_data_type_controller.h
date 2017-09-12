// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/async_directory_type_controller.h"
#include "components/sync/driver/sync_api_component_factory.h"

namespace browser_sync {

// A class that manages the startup and shutdown of typed_url sync.
class TypedUrlDataTypeController : public syncer::AsyncDirectoryTypeController {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  TypedUrlDataTypeController(const base::Closure& dump_stack,
                             syncer::SyncClient* sync_client,
                             const char* history_disabled_pref_name);
  ~TypedUrlDataTypeController() override;

  // AsyncDirectoryTypeController implementation.
  bool ReadyForStart() const override;

 protected:
  // AsyncDirectoryTypeController implementation.
  bool PostTaskOnModelThread(const base::Location& from_here,
                             const base::Closure& task) override;

 private:
  void OnSavingBrowserHistoryDisabledChanged();

  // Name of the pref that indicates whether saving history is disabled.
  const char* history_disabled_pref_name_;

  PrefChangeRegistrar pref_registrar_;

  // Helper object to make sure we don't leave tasks running on the history
  // thread.
  base::CancelableTaskTracker task_tracker_;

  syncer::SyncClient* const sync_client_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_DATA_TYPE_CONTROLLER_H__
