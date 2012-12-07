// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/api_resource_event_notifier.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char kEventTypeKey[] = "type";

const char kSrcIdKey[] = "srcId";
const char kIsFinalEventKey[] = "isFinalEvent";
const char kResultCodeKey[] = "resultCode";

ApiResourceEventNotifier::ApiResourceEventNotifier(
    EventRouter* router,
    Profile* profile,
    const std::string& src_extension_id,
    int src_id,
    const GURL& src_url)
    : router_(router),
      profile_(profile),
      src_extension_id_(src_extension_id),
      src_id_(src_id),
      src_url_(src_url) {
}

// static
std::string ApiResourceEventNotifier::ApiResourceEventTypeToString(
    ApiResourceEventType event_type) {
  NOTREACHED();
  return std::string();
}

ApiResourceEventNotifier::~ApiResourceEventNotifier() {}

void ApiResourceEventNotifier::DispatchEvent(
    const std::string& event_name, DictionaryValue* args) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &ApiResourceEventNotifier::DispatchEventOnUIThread, this,
          event_name, args));
}

void ApiResourceEventNotifier::DispatchEventOnUIThread(
    const std::string& event_name, DictionaryValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<ListValue> arguments(new ListValue());
  arguments->Set(0, args);
  scoped_ptr<Event> event(new Event(event_name, arguments.Pass()));
  event->restrict_to_profile = profile_;
  event->event_url = src_url_;
  router_->DispatchEventToExtension(src_extension_id_, event.Pass());
}

DictionaryValue* ApiResourceEventNotifier::CreateApiResourceEvent(
    ApiResourceEventType event_type) {
  DictionaryValue* event = new DictionaryValue();
  event->SetString(kEventTypeKey, ApiResourceEventTypeToString(event_type));
  event->SetInteger(kSrcIdKey, src_id_);

  // TODO(miket): Signal that it's OK to clean up onEvent listeners. This is
  // the framework we'll use, but we need to start using it.
  event->SetBoolean(kIsFinalEventKey, false);

  // The caller owns the created event, which typically is then given to a
  // ListValue to dispose of.
  return event;
}

}  // namespace extensions
