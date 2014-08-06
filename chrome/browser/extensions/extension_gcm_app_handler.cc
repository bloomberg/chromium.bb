// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_gcm_app_handler.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/extensions/api/gcm/gcm_api.h"
#endif

namespace extensions {

namespace {

const char kDummyAppId[] = "extension.guard.dummy.id";

base::LazyInstance<BrowserContextKeyedAPIFactory<ExtensionGCMAppHandler> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

bool IsGCMPermissionEnabled(const Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(APIPermission::kGcm);
}

}  // namespace


// static
BrowserContextKeyedAPIFactory<ExtensionGCMAppHandler>*
ExtensionGCMAppHandler::GetFactoryInstance() {
  return g_factory.Pointer();
}

ExtensionGCMAppHandler::ExtensionGCMAppHandler(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      extension_registry_observer_(this),
      weak_factory_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
#if !defined(OS_ANDROID)
  js_event_router_.reset(new extensions::GcmJsEventRouter(profile_));
#endif
}

ExtensionGCMAppHandler::~ExtensionGCMAppHandler() {
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  for (ExtensionSet::const_iterator extension = enabled_extensions.begin();
       extension != enabled_extensions.end();
       ++extension) {
    if (IsGCMPermissionEnabled(extension->get()))
      GetGCMDriver()->RemoveAppHandler((*extension)->id());
  }
}

void ExtensionGCMAppHandler::ShutdownHandler() {
#if !defined(OS_ANDROID)
  js_event_router_.reset();
#endif
}

void ExtensionGCMAppHandler::OnMessage(
    const std::string& app_id,
    const gcm::GCMClient::IncomingMessage& message) {
#if !defined(OS_ANDROID)
  js_event_router_->OnMessage(app_id, message);
#endif
}

void ExtensionGCMAppHandler::OnMessagesDeleted(const std::string& app_id) {
#if !defined(OS_ANDROID)
  js_event_router_->OnMessagesDeleted(app_id);
#endif
}

void ExtensionGCMAppHandler::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {
#if !defined(OS_ANDROID)
  js_event_router_->OnSendError(app_id, send_error_details);
#endif
}

void ExtensionGCMAppHandler::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  // This event is not exposed to JS API. It terminates here.
}

void ExtensionGCMAppHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (IsGCMPermissionEnabled(extension))
    AddAppHandler(extension->id());
}

void ExtensionGCMAppHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  if (!IsGCMPermissionEnabled(extension))
    return;

  if (reason == UnloadedExtensionInfo::REASON_UPDATE &&
      GetGCMDriver()->app_handlers().size() == 1) {
    // When the extension is being updated, it will be first unloaded and then
    // loaded again by ExtensionService::AddExtension. If the app handler for
    // this extension is the only handler, removing it and adding it again will
    // cause the GCM service being stopped and restarted unnecessarily. To work
    // around this, we add a dummy app handler to guard against it. This dummy
    // app handler will be removed once the extension loading logic is done.
    //
    // Also note that the GCM message routing will not be interruptted during
    // the update process since unloading and reloading extension are done in
    // the single function ExtensionService::AddExtension.
    AddDummyAppHandler();

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionGCMAppHandler::RemoveDummyAppHandler,
                   weak_factory_.GetWeakPtr()));
  }

  RemoveAppHandler(extension->id());
}

void ExtensionGCMAppHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  if (IsGCMPermissionEnabled(extension)) {
    GetGCMDriver()->Unregister(
        extension->id(),
        base::Bind(&ExtensionGCMAppHandler::OnUnregisterCompleted,
                   weak_factory_.GetWeakPtr(),
                   extension->id()));
    RemoveAppHandler(extension->id());
  }
}

void ExtensionGCMAppHandler::AddDummyAppHandler() {
  AddAppHandler(kDummyAppId);
}

void ExtensionGCMAppHandler::RemoveDummyAppHandler() {
  RemoveAppHandler(kDummyAppId);
}

gcm::GCMDriver* ExtensionGCMAppHandler::GetGCMDriver() const {
  return gcm::GCMProfileServiceFactory::GetForProfile(profile_)->driver();
}

void ExtensionGCMAppHandler::OnUnregisterCompleted(
    const std::string& app_id, gcm::GCMClient::Result result) {
  // Nothing to do.
}

void ExtensionGCMAppHandler::AddAppHandler(const std::string& app_id) {
  GetGCMDriver()->AddAppHandler(app_id, this);
}

void ExtensionGCMAppHandler::RemoveAppHandler(const std::string& app_id) {
  GetGCMDriver()->RemoveAppHandler(app_id);
}

}  // namespace extensions
