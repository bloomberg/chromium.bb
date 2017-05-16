// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/driver/async_directory_type_controller.h"

namespace autofill {
class AutofillWebDataService;
}  // namespace autofill

namespace browser_sync {

// A class that manages the startup and shutdown of autofill sync.
class AutofillDataTypeController : public syncer::AsyncDirectoryTypeController {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  AutofillDataTypeController(
      scoped_refptr<base::SingleThreadTaskRunner> db_thread,
      const base::Closure& dump_stack,
      syncer::SyncClient* sync_client,
      const scoped_refptr<autofill::AutofillWebDataService>& web_data_service);
  ~AutofillDataTypeController() override;

 protected:
  // AsyncDirectoryTypeController implementation.
  bool StartModels() override;

 private:
  friend class AutofillDataTypeControllerTest;
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSReady);
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSNotReady);

  // Callback once WebDatabase has loaded.
  void WebDatabaseLoaded();

  // A reference to the AutofillWebDataService for this controller.
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_DATA_TYPE_CONTROLLER_H__
