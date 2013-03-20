// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_input_module_constants.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/mime_types_handler.h"
#include "content/public/browser/stream_handle.h"

namespace keys = extension_input_module_constants;

namespace events {

const char kOnExecuteMimeTypeHandler[] =
    "streamsPrivate.onExecuteMimeTypeHandler";

}  // namespace events

namespace extensions {

// static
StreamsPrivateAPI* StreamsPrivateAPI::Get(Profile* profile) {
  return GetFactoryInstance()->GetForProfile(profile);
}

StreamsPrivateAPI::StreamsPrivateAPI(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  (new MimeTypesHandlerParser)->Register();

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

StreamsPrivateAPI::~StreamsPrivateAPI() {
}

void StreamsPrivateAPI::ExecuteMimeTypeHandler(
    const std::string& extension_id,
    scoped_ptr<content::StreamHandle> stream) {
  // Create the event's arguments value.
  scoped_ptr<ListValue> event_args(new ListValue());
  event_args->Append(new base::StringValue(stream->GetMimeType()));
  event_args->Append(new base::StringValue(stream->GetOriginalURL().spec()));
  event_args->Append(new base::StringValue(stream->GetURL().spec()));

  scoped_ptr<Event> event(new Event(events::kOnExecuteMimeTypeHandler,
                                    event_args.Pass()));

  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      extension_id, event.Pass());

  streams_[extension_id][stream->GetURL()] =
      make_linked_ptr(stream.release());
}

static base::LazyInstance<ProfileKeyedAPIFactory<StreamsPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<StreamsPrivateAPI>*
    StreamsPrivateAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

void StreamsPrivateAPI::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    const Extension* extension =
        content::Details<const UnloadedExtensionInfo>(details)->extension;
    streams_.erase(extension->id());
  }
}
}  // namespace extensions
