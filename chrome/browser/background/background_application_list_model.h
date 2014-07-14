// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_BACKGROUND_APPLICATION_LIST_MODEL_H_
#define CHROME_BROWSER_BACKGROUND_BACKGROUND_APPLICATION_LIST_MODEL_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/extension.h"

class Profile;

namespace gfx {
class ImageSkia;
}

// Model for list of Background Applications associated with a Profile (i.e.
// extensions with kBackgroundPermission set, or hosted apps with a
// BackgroundContents).
class BackgroundApplicationListModel : public content::NotificationObserver {
 public:
  // Observer is informed of changes to the model.  Users of the
  // BackgroundApplicationListModel should anticipate that associated data,
  // e. g. the Icon, may exist and yet not be immediately available.  When the
  // data becomes available, OnApplicationDataChanged will be invoked for all
  // Observers of the model.
  class Observer {
   public:
    // Invoked when data that the model associates with the extension, such as
    // the Icon, has changed.
    virtual void OnApplicationDataChanged(
        const extensions::Extension* extension,
        Profile* profile);

    // Invoked when the model detects a previously unknown extension and/or when
    // it no longer detects a previously known extension.
    virtual void OnApplicationListChanged(Profile* profile);

   protected:
    virtual ~Observer();
  };

  // Create a new model associated with profile.
  explicit BackgroundApplicationListModel(Profile* profile);

  virtual ~BackgroundApplicationListModel();

  // Associate observer with this model.
  void AddObserver(Observer* observer);

  // Return the icon associated with |extension| or NULL.  NULL indicates either
  // that there is no icon associated with the extension, or that a pending
  // task to retrieve the icon has not completed.  See the Observer class above.
  //
  // NOTE: The model manages the ImageSkia result, that is it "owns" the memory,
  //       releasing it if the associated background application is unloaded.
  // NOTE: All icons are currently sized as
  //       ExtensionIconSet::EXTENSION_ICON_BITTY.
  const gfx::ImageSkia* GetIcon(const extensions::Extension* extension);

  // Return the position of |extension| within this list model.
  int GetPosition(const extensions::Extension* extension) const;

  // Return the extension at the specified |position| in this list model.
  const extensions::Extension* GetExtension(int position) const;

  // Returns true if the passed extension is a background app.
  static bool IsBackgroundApp(const extensions::Extension& extension,
                              Profile* profile);

  // Dissociate observer from this model.
  void RemoveObserver(Observer* observer);

  extensions::ExtensionList::const_iterator begin() const {
    return extensions_.begin();
  }

  extensions::ExtensionList::const_iterator end() const {
    return extensions_.end();
  }

  size_t size() const {
    return extensions_.size();
  }

  // Returns true if all startup notifications have already been issued.
  bool is_ready() const {
    return ready_;
  }

 private:
  // Contains data associated with a background application that is not
  // represented by the Extension class.
  class Application;

  // Associates extension id strings with Application objects.
  typedef std::map<std::string, Application*> ApplicationMap;

  // Identifies and caches data related to the extension.
  void AssociateApplicationData(const extensions::Extension* extension);

  // Clears cached data related to |extension|.
  void DissociateApplicationData(const extensions::Extension* extension);

  // Returns the Application associated with |extension| or NULL.
  const Application* FindApplication(
      const extensions::Extension* extension) const;

  // Returns the Application associated with |extension| or NULL.
  Application* FindApplication(const extensions::Extension* extension);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Notifies observers that some of the data associated with this background
  // application, e. g. the Icon, has changed.
  void SendApplicationDataChangedNotifications(
      const extensions::Extension* extension);

  // Notifies observers that at least one background application has been added
  // or removed.
  void SendApplicationListChangedNotifications();

  // Invoked by Observe for NOTIFICATION_EXTENSION_LOADED_DEPRECATED.
  void OnExtensionLoaded(const extensions::Extension* extension);

  // Invoked by Observe for NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED.
  void OnExtensionUnloaded(const extensions::Extension* extension);

  // Invoked by Observe for NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED.
  void OnExtensionPermissionsUpdated(
      const extensions::Extension* extension,
      extensions::UpdatedExtensionPermissionsInfo::Reason reason,
      const extensions::PermissionSet* permissions);

  // Refresh the list of background applications and generate notifications.
  void Update();

  // Determines if the given extension has to be considered a "background app"
  // due to its use of PushMessaging. Normally every extension that expectes
  // push messages is classified as "background app", however there are some
  // rare exceptions, so this function implements a whitelist.
  static bool RequiresBackgroundModeForPushMessaging(
      const extensions::Extension& extension);

  ApplicationMap applications_;
  extensions::ExtensionList extensions_;
  ObserverList<Observer, true> observers_;
  Profile* profile_;
  content::NotificationRegistrar registrar_;
  bool ready_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundApplicationListModel);
};

#endif  // CHROME_BROWSER_BACKGROUND_BACKGROUND_APPLICATION_LIST_MODEL_H_
