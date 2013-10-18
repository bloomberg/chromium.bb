// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_event_router.h"

#include <set>

#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/api/identity.h"

namespace extensions {

IdentityEventRouter::IdentityEventRouter(Profile* profile)
    : profile_(profile) {}

IdentityEventRouter::~IdentityEventRouter() {}

void IdentityEventRouter::DispatchSignInEvent(const std::string& id,
                                              const std::string& email,
                                              bool is_signed_in) {
  const EventListenerMap::ListenerList& listeners =
      extensions::ExtensionSystem::Get(profile_)->event_router()->listeners()
      .GetEventListenersByName(api::identity::OnSignInChanged::kEventName);

  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();
  EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();

  api::identity::AccountInfo account_info;
  account_info.id = id;

  api::identity::AccountInfo account_info_email;
  account_info_email.id = id;
  account_info_email.email = scoped_ptr<std::string>(new std::string(email));

  std::set<std::string> already_dispatched;

  for (EventListenerMap::ListenerList::const_iterator it = listeners.begin();
       it != listeners.end();
       ++it) {

    const std::string extension_id = (*it)->extension_id;
    const Extension* extension = service->extensions()->GetByID(extension_id);

    if (ContainsKey(already_dispatched, extension_id))
      continue;

    already_dispatched.insert(extension_id);

    // Add the email address to AccountInfo only for extensions that
    // have APIPermission::kIdentityEmail.
    scoped_ptr<base::ListValue> args;
    if (extension->HasAPIPermission(APIPermission::kIdentityEmail)) {
      args = api::identity::OnSignInChanged::Create(account_info_email,
                                                    is_signed_in);
    } else {
      args = api::identity::OnSignInChanged::Create(account_info,
                                                    is_signed_in);
    }

    scoped_ptr<Event> event(
        new Event(api::identity::OnSignInChanged::kEventName, args.Pass()));
    event->restrict_to_profile = profile_;
    event_router->DispatchEventToExtension(extension_id, event.Pass());
  }
}

}  // namespace extensions
