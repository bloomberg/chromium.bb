// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"

#include <set>

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/experimental_push_messaging.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"

namespace extensions {

namespace glue = api::experimental_push_messaging;

PushMessagingEventRouter::PushMessagingEventRouter(Profile* profile)
    : profile_(profile) {
}

PushMessagingEventRouter::~PushMessagingEventRouter() {}

void PushMessagingEventRouter::Init() {
  ProfileSyncService* pss = ProfileSyncServiceFactory::GetForProfile(profile_);
  // This may be NULL; for example, for the ChromeOS guest user. In these cases,
  // just return without setting up anything, since it won't work anyway.
  if (!pss)
    return;

  const ExtensionSet* extensions =
      ExtensionSystem::Get(profile_)->extension_service()->extensions();
  std::set<std::string> push_messaging_extensions;
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const Extension* extension = *it;
    if (extension->HasAPIPermission(APIPermission::kPushMessaging)) {
      push_messaging_extensions.insert(extension->id());
    }
  }
  handler_.reset(new PushMessagingInvalidationHandler(
      pss, this, push_messaging_extensions));

  // Register for extension load/unload as well, so we can update any
  // registrations as appropriate.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
}

void PushMessagingEventRouter::Shutdown() {
  // We need an explicit Shutdown() due to the dependencies among the various
  // ProfileKeyedServices. ProfileSyncService depends on ExtensionSystem, so
  // it is destroyed before us in the destruction phase of Profile shutdown.
  // As a result, we need to drop any references to it in the Shutdown() phase
  // instead.
  handler_.reset();
}

void PushMessagingEventRouter::SetMapperForTest(
    scoped_ptr<PushMessagingInvalidationMapper> mapper) {
  handler_ = mapper.Pass();
}

void PushMessagingEventRouter::TriggerMessageForTest(
    const std::string& extension_id,
    int subchannel,
    const std::string& payload) {
  OnMessage(extension_id, subchannel, payload);
}

void PushMessagingEventRouter::OnMessage(const std::string& extension_id,
                                         int subchannel,
                                         const std::string& payload) {
  glue::Message message;
  message.subchannel_id = subchannel;
  message.payload = payload;

  scoped_ptr<base::ListValue> args(glue::OnMessage::Create(message));
  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      extension_id,
      event_names::kOnPushMessage,
      args.Pass(),
      profile_,
      GURL());
}

void PushMessagingEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension = content::Details<Extension>(details).ptr();
      if (extension->HasAPIPermission(APIPermission::kPushMessaging)) {
        handler_->RegisterExtension(extension->id());
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      if (extension->HasAPIPermission(APIPermission::kPushMessaging)) {
        handler_->UnregisterExtension(extension->id());
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace extensions
