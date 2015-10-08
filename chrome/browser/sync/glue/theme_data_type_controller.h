// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_THEME_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_THEME_DATA_TYPE_CONTROLLER_H_

#include "components/sync_driver/ui_data_type_controller.h"

class Profile;

namespace browser_sync {

class ThemeDataTypeController : public sync_driver::UIDataTypeController {
 public:
  ThemeDataTypeController(const base::Closure& error_callback,
                          sync_driver::SyncClient* sync_client,
                          Profile* profile);

 private:
  ~ThemeDataTypeController() override;

  // UIDataTypeController implementations.
  bool StartModels() override;

  Profile* const profile_;
  DISALLOW_COPY_AND_ASSIGN(ThemeDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_THEME_DATA_TYPE_CONTROLLER_H_
