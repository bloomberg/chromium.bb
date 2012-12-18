// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host.h"

namespace extensions {

class ExtensionSystem;

namespace api {

namespace developer_private {
struct ItemInfo;
struct ItemInspectView;
}

}  // namespace api

}  // namespace extensions

namespace developer = extensions::api::developer_private;

typedef std::vector<linked_ptr<developer::ItemInfo> > ItemInfoList;
typedef std::vector<linked_ptr<developer::ItemInspectView> >
    ItemInspectViewList;

namespace extensions {

// The profile-keyed service that manages the DeveloperPrivate API.
class DeveloperPrivateAPI : public ProfileKeyedService,
                            public content::NotificationObserver {
 public:
  // Convenience method to get the DeveloperPrivateAPI for a profile.
  static DeveloperPrivateAPI* Get(Profile* profile);

  explicit DeveloperPrivateAPI(Profile* profile);
  virtual ~DeveloperPrivateAPI();

  void AddItemsInfo(const ExtensionSet& items,
                    ExtensionSystem* system,
                    ItemInfoList* item_list);

  // ProfileKeyedService implementation
  virtual void Shutdown() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void RegisterNotifications();

  scoped_ptr<developer::ItemInfo> CreateItemInfo(
      const extensions::Extension& item,
      ExtensionSystem* system,
      bool item_is_enabled);

  // Helper that lists the current inspectable html pages for the extension.
  void GetInspectablePagesForExtensionProcess(
      const std::set<content::RenderViewHost*>& views,
      ItemInspectViewList* result);

  ItemInspectViewList GetInspectablePagesForExtension(
      const extensions::Extension* extension,
      bool extension_is_enabled);

  Profile* profile_;

  content::NotificationRegistrar registrar_;

  // The page may be refreshed in response to a RENDER_VIEW_HOST_DELETED,
  // but the iteration over RenderViewHosts will include the host because the
  // notification is sent when it is in the process of being deleted (and before
  // it is removed from the process). Keep a pointer to it so we can exclude
  // it from the active views.
  content::RenderViewHost* deleting_render_view_host_;
};

namespace api {

class DeveloperPrivateAutoUpdateFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("developerPrivate.autoUpdate");

 protected:
  virtual ~DeveloperPrivateAutoUpdateFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class DeveloperPrivateGetItemsInfoFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("developerPrivate.getItemsInfo");

 protected:
  virtual ~DeveloperPrivateGetItemsInfoFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class DeveloperPrivateInspectFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("developerPrivate.inspect");

 protected:
  virtual ~DeveloperPrivateInspectFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
