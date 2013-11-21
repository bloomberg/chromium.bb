// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_

#include "base/memory/scoped_ptr.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ExtensionAppModelBuilder;
class ExtensionService;
class Profile;

namespace app_list {

class AppListModel;

// Keyed Service that owns, stores, and syncs an AppListModel for an
// ExtensionSystem and corresponding profile.
// TODO(stevenjb): Integrate with Sync. crbug.com/305024.
class AppListSyncableService : public BrowserContextKeyedService,
                               public content::NotificationObserver {
 public:
  // Create an empty model. Then, if |extension_service| is non-NULL and ready,
  // populate it. Otherwise populate the model once extensions become ready.
  AppListSyncableService(Profile* profile, ExtensionService* extension_service);

  virtual ~AppListSyncableService();

  AppListModel* model() { return model_.get(); }

 private:
  void BuildModel();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<AppListModel> model_;
  scoped_ptr<ExtensionAppModelBuilder> apps_builder_;

  DISALLOW_COPY_AND_ASSIGN(AppListSyncableService);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
