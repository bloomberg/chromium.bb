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
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/streams_private.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/stream_handle.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

namespace streams_private = api::streams_private;

// static
StreamsPrivateAPI* StreamsPrivateAPI::Get(content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

StreamsPrivateAPI::StreamsPrivateAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)), weak_ptr_factory_(this) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::Source<Profile>(profile_));
}

StreamsPrivateAPI::~StreamsPrivateAPI() {
}

void StreamsPrivateAPI::ExecuteMimeTypeHandler(
    const std::string& extension_id,
    const content::WebContents* web_contents,
    scoped_ptr<content::StreamHandle> stream,
    int64 expected_content_size) {
  // Create the event's arguments value.
  streams_private::StreamInfo info;
  info.mime_type = stream->GetMimeType();
  info.original_url = stream->GetOriginalURL().spec();
  info.stream_url = stream->GetURL().spec();
  info.tab_id = ExtensionTabUtil::GetTabId(web_contents);

  int size = -1;
  if (expected_content_size <= INT_MAX)
    size = expected_content_size;
  info.expected_content_size = size;
  info.response_headers = stream->GetResponseHeaders();

  scoped_ptr<Event> event(
      new Event(streams_private::OnExecuteMimeTypeHandler::kEventName,
                streams_private::OnExecuteMimeTypeHandler::Create(info)));

  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      extension_id, event.Pass());

  GURL url = stream->GetURL();
  streams_[extension_id][url] = make_linked_ptr(stream.release());
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<StreamsPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<StreamsPrivateAPI>*
StreamsPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void StreamsPrivateAPI::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED) {
    const Extension* extension =
        content::Details<const UnloadedExtensionInfo>(details)->extension;
    streams_.erase(extension->id());
  }
}
}  // namespace extensions
