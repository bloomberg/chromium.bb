// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/app_runtime/app_runtime_api.h"

#include "base/json/json_writer.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/web_intent_data.h"

namespace {

const char kOnLaunchedEvent[] = "app.runtime.onLaunched";

}  // anonymous namespace

namespace extensions {

// static.
void AppEventRouter::DispatchOnLaunchedEvent(
    Profile* profile, const Extension* extension) {
  scoped_ptr<ListValue> arguments(new ListValue());
  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension->id(), kOnLaunchedEvent, arguments.Pass(), NULL, GURL());
}

// static.
void AppEventRouter::DispatchOnLaunchedEventWithFileEntry(
    Profile* profile, const Extension* extension, const string16& action,
    const std::string& file_system_id, const std::string& base_name) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* launch_data = new DictionaryValue();
  DictionaryValue* intent = new DictionaryValue();
  intent->SetString("action", UTF16ToUTF8(action));
  intent->SetString("type", "chrome-extension://fileentry");
  launch_data->Set("intent", intent);
  args->Append(launch_data);
  DictionaryValue* intent_data = new DictionaryValue();
  intent_data->SetString("format", "fileEntry");
  intent_data->SetString("fileSystemId", file_system_id);
  intent_data->SetString("baseName", base_name);
  // NOTE: This second argument is dropped before being dispatched to the client
  // code.
  args->Append(intent_data);
  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension->id(), kOnLaunchedEvent, args.Pass(), NULL, GURL());
}

// static.
void AppEventRouter::DispatchOnLaunchedEventWithWebIntent(
    Profile* profile, const Extension* extension,
    const webkit_glue::WebIntentData web_intent_data) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* launch_data = new DictionaryValue();
  DictionaryValue* intent = new DictionaryValue();
  intent->SetString("action", UTF16ToUTF8(web_intent_data.action));
  intent->SetString("type", UTF16ToUTF8(web_intent_data.type));
  launch_data->Set("intent", intent);
  args->Append(launch_data);
  DictionaryValue* intent_data;
  switch (web_intent_data.data_type) {
    case webkit_glue::WebIntentData::SERIALIZED:
      intent_data = new DictionaryValue();
      intent_data->SetString("format", "serialized");
      intent_data->SetString("data", UTF16ToUTF8(web_intent_data.data));
      // NOTE: This second argument is dropped before being dispatched to the
      // client code.
      args->Append(intent_data);
      break;
    case webkit_glue::WebIntentData::UNSERIALIZED:
      intent->SetString("data", UTF16ToUTF8(web_intent_data.unserialized_data));
      break;
    case webkit_glue::WebIntentData::BLOB:
      intent_data = new DictionaryValue();
      intent_data->SetString("format", "blob");
      intent_data->SetString("blobFileName", web_intent_data.blob_file.value());
      intent_data->SetString("blobLength",
                             base::Int64ToString(web_intent_data.blob_length));
      // NOTE: This second argument is dropped before being dispatched to the
      // client code.
      args->Append(intent_data);
      break;
    case webkit_glue::WebIntentData::FILESYSTEM:
      intent_data = new DictionaryValue();
      intent_data->SetString("format", "filesystem");
      intent_data->SetString("fileSystemId", web_intent_data.filesystem_id);
      intent_data->SetString("baseName", web_intent_data.root_name);
      args->Append(intent_data);
      break;
    default:
      NOTREACHED();
      break;
  }
  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension->id(), kOnLaunchedEvent, args.Pass(), NULL, GURL());
}

}  // namespace extensions
