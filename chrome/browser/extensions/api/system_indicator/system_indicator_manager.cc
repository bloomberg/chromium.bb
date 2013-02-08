// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager.h"

#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/common/extensions/api/system_indicator.h"
#include "chrome/common/extensions/extension.h"
#include "ui/gfx/image/image.h"

namespace extensions {

// Observes clicks on a given status icon and forwards the event to the
// appropriate extension.  Handles icon updates, and responsible for creating
// and removing the icon from the notification area during construction and
// destruction.
class ExtensionIndicatorIcon : public StatusIconObserver,
                               public ExtensionActionIconFactory::Observer {
 public:
  ExtensionIndicatorIcon(const Extension* extension,
                         const ExtensionAction* action,
                         Profile* profile,
                         StatusTray* status_tray,
                         StatusIcon* status_icon);
  virtual ~ExtensionIndicatorIcon();

  // StatusIconObserver implementation.
  virtual void OnStatusIconClicked() OVERRIDE;

  // ExtensionActionIconFactory::Observer implementation.
  virtual void OnIconUpdated() OVERRIDE;

 private:
  const extensions::Extension* extension_;
  StatusTray* status_tray_;
  StatusIcon* icon_;
  Profile* profile_;
  ExtensionActionIconFactory icon_factory_;
};

ExtensionIndicatorIcon::ExtensionIndicatorIcon(const Extension* extension,
                                               const ExtensionAction* action,
                                               Profile* profile,
                                               StatusTray* status_tray,
                                               StatusIcon* status_icon)
    : extension_(extension),
      status_tray_(status_tray),
      icon_(status_icon),
      profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          icon_factory_(profile, extension, action, this)) {
  icon_->AddObserver(this);
  OnIconUpdated();
}

ExtensionIndicatorIcon::~ExtensionIndicatorIcon() {
  icon_->RemoveObserver(this);
  status_tray_->RemoveStatusIcon(icon_);
}

void ExtensionIndicatorIcon::OnStatusIconClicked() {
  scoped_ptr<base::ListValue> params(
      api::system_indicator::OnClicked::Create());

  EventRouter* event_router =
      ExtensionSystem::Get(profile_)->event_router();
  scoped_ptr<Event> event(new Event(
      event_names::kOnSystemIndicatorClicked,
      params.Pass(),
      profile_));
  event_router->DispatchEventToExtension(
      extension_->id(), event.Pass());
}

void ExtensionIndicatorIcon::OnIconUpdated() {
  icon_->SetImage(
      icon_factory_.GetIcon(ExtensionAction::kDefaultTabId).AsImageSkia());
}

SystemIndicatorManager::SystemIndicatorManager(Profile* profile,
                                               StatusTray* status_tray)
    : profile_(profile),
      status_tray_(status_tray) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_SYSTEM_INDICATOR_UPDATED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
}

SystemIndicatorManager::~SystemIndicatorManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SystemIndicatorManager::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SystemIndicatorManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      RemoveIndicator(
          content::Details<UnloadedExtensionInfo>(details)->extension->id());
      break;
    case chrome::NOTIFICATION_EXTENSION_SYSTEM_INDICATOR_UPDATED:
      OnSystemIndicatorChanged(
          content::Details<ExtensionAction>(details).ptr());
      break;
    default:
      NOTREACHED();
      break;
  }
}

void SystemIndicatorManager::OnSystemIndicatorChanged(
    const ExtensionAction* extension_action) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string extension_id = extension_action->extension_id();
  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();

  if (extension_action->GetIsVisible(ExtensionAction::kDefaultTabId)) {
    const Extension* extension =
        service->GetExtensionById(extension_id, false);
    CreateOrUpdateIndicator(extension, extension_action);
  } else {
    RemoveIndicator(extension_id);
  }
}

bool SystemIndicatorManager::SendClickEventToExtensionForTest(
    const std::string extension_id) {

    extensions::SystemIndicatorManager::SystemIndicatorMap::iterator it =
        system_indicators_.find(extension_id);

    if (it == system_indicators_.end())
      return false;

    it->second->OnStatusIconClicked();
    return true;
}

void SystemIndicatorManager::CreateOrUpdateIndicator(
    const Extension* extension,
    const ExtensionAction* extension_action) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SystemIndicatorMap::iterator it = system_indicators_.find(extension->id());
  if (it != system_indicators_.end()) {
    it->second->OnIconUpdated();
    return;
  }

  StatusIcon* indicator_icon = status_tray_->CreateStatusIcon();
  if (indicator_icon != NULL) {
    ExtensionIndicatorIcon* status_icon = new ExtensionIndicatorIcon(
        extension,
        extension_action,
        profile_,
        status_tray_,
        indicator_icon);
    system_indicators_[extension->id()] = make_linked_ptr(status_icon);
  }
}

void SystemIndicatorManager::RemoveIndicator(const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  system_indicators_.erase(extension_id);
}

}  // namespace extensions
