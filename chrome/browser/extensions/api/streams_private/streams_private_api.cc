// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/extensions/api/streams_private.h"
#include "content/public/browser/stream_handle.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_registry.h"
#include "net/http/http_response_headers.h"

namespace {

void CreateResponseHeadersDictionary(const net::HttpResponseHeaders* headers,
                                     base::DictionaryValue* result) {
  if (!headers)
    return;

  void* iter = NULL;
  std::string header_name;
  std::string header_value;
  while (headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
    base::Value* existing_value = NULL;
    if (result->Get(header_name, &existing_value)) {
      base::StringValue* existing_string_value =
          static_cast<base::StringValue*>(existing_value);
      existing_string_value->GetString()->append(", ").append(header_value);
    } else {
      result->SetString(header_name, header_value);
    }
  }
}

}  // namespace

namespace extensions {

namespace streams_private = api::streams_private;

// static
StreamsPrivateAPI* StreamsPrivateAPI::Get(content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

StreamsPrivateAPI::StreamsPrivateAPI(content::BrowserContext* context)
    : browser_context_(context),
      weak_ptr_factory_(this),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
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

  CreateResponseHeadersDictionary(stream->GetResponseHeaders().get(),
                                  &info.response_headers.additional_properties);

  scoped_ptr<Event> event(
      new Event(streams_private::OnExecuteMimeTypeHandler::kEventName,
                streams_private::OnExecuteMimeTypeHandler::Create(info)));

  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, event.Pass());

  GURL url = stream->GetURL();
  streams_[extension_id][url] = make_linked_ptr(stream.release());
}

void StreamsPrivateAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  streams_.erase(extension->id());
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<StreamsPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<StreamsPrivateAPI>*
StreamsPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
