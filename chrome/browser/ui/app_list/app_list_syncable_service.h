// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_

#include "base/memory/scoped_ptr.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class ExtensionAppModelBuilder;
class Profile;

namespace app_list {

class AppListModel;

// Keyed Service that owns, stores, and syncs an AppListModel for a profile.
// TODO(stevenjb): Integrate with Sync. crbug.com/305024.
class AppListSyncableService : public BrowserContextKeyedService {
 public:
  explicit AppListSyncableService(Profile* profile);
  virtual ~AppListSyncableService();

  AppListModel* model() { return model_.get(); }

 private:
  Profile* profile_;
  scoped_ptr<AppListModel> model_;
  scoped_ptr<ExtensionAppModelBuilder> apps_builder_;

  DISALLOW_COPY_AND_ASSIGN(AppListSyncableService);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
