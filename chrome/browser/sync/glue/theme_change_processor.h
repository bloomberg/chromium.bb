// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_THEME_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_THEME_CHANGE_PROCESSOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_error_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"

class Profile;

namespace browser_sync {

class DataTypeErrorHandler;

// This class is responsible for taking changes from the
// ThemeService and applying them to the csync 'syncable'
// model, and vice versa. All operations and use of this class are
// from the UI thread.
class ThemeChangeProcessor : public ChangeProcessor,
                             public content::NotificationObserver {
 public:
  explicit ThemeChangeProcessor(DataTypeErrorHandler* error_handler);
  virtual ~ThemeChangeProcessor();

  // content::NotificationObserver implementation.
  // ThemeService -> csync model change application.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ChangeProcessor implementation.
  // csync model -> ThemeService change application.
  virtual void ApplyChangesFromSyncModel(
      const csync::BaseTransaction* trans,
      const csync::ImmutableChangeRecordList& changes) OVERRIDE;

 protected:
  // ChangeProcessor implementation.
  virtual void StartImpl(Profile* profile) OVERRIDE;
  virtual void StopImpl() OVERRIDE;

 private:
  friend class ScopedStopObserving<ThemeChangeProcessor>;
  void StartObserving();
  void StopObserving();

  content::NotificationRegistrar notification_registrar_;
  // Profile associated with the ThemeService.  Non-NULL iff |running()| is
  // true.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ThemeChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_THEME_CHANGE_PROCESSOR_H_
