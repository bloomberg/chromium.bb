// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/stream_handle.h"
#include "extensions/browser/event_router.h"

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
      weak_ptr_factory_(this) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

StreamsPrivateAPI::~StreamsPrivateAPI() {
}

void StreamsPrivateAPI::ExecuteMimeTypeHandler(
    const std::string& extension_id,
    const content::WebContents* web_contents,
    scoped_ptr<content::StreamHandle> stream,
    int64 expected_content_size) {
  // Create the event's arguments value.
  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  event_args->Append(new base::StringValue(stream->GetMimeType()));
  event_args->Append(new base::StringValue(stream->GetOriginalURL().spec()));
  event_args->Append(new base::StringValue(stream->GetURL().spec()));
  event_args->Append(
      new base::FundamentalValue(ExtensionTabUtil::GetTabId(web_contents)));

  int size = -1;
  if (expected_content_size <= INT_MAX)
    size = expected_content_size;
  event_args->Append(new base::FundamentalValue(size));

  scoped_ptr<Event> event(new Event(events::kOnExecuteMimeTypeHandler,
                                    event_args.Pass()));

  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      extension_id, event.Pass());

  GURL url = stream->GetURL();
  streams_[extension_id][url] = make_linked_ptr(stream.release());
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
