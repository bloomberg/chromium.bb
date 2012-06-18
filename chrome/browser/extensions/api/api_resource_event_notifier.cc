// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/api_resource_event_notifier.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace events {
const char kExperimentalSocketOnEvent[] = "experimental.socket.onEvent";
const char kExperimentalUsbOnEvent[] = "experimental.usb.onEvent";
};

namespace extensions {

const char kEventTypeKey[] = "type";

const char kEventTypeConnectComplete[] = "connectComplete";
const char kEventTypeDataRead[] = "dataRead";
const char kEventTypeWriteComplete[] = "writeComplete";

const char kEventTypeTransferComplete[] = "transferComplete";

const char kSrcIdKey[] = "srcId";
const char kIsFinalEventKey[] = "isFinalEvent";

const char kResultCodeKey[] = "resultCode";
const char kDataKey[] = "data";
const char kAddressKey[] = "address";
const char kPortKey[] = "port";

APIResourceEventNotifier::APIResourceEventNotifier(
    ExtensionEventRouter* router,
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

void APIResourceEventNotifier::OnConnectComplete(int result_code) {
  SendEventWithResultCode(events::kExperimentalSocketOnEvent,
                          API_RESOURCE_EVENT_CONNECT_COMPLETE, result_code);
}

void APIResourceEventNotifier::OnDataRead(int result_code,
                                          base::ListValue* data,
                                          const std::string& address,
                                          int port) {
  // Do we have a destination for this event? There will be one if a source id
  // was injected by the request handler for the resource's create method in
  // schema_generated_bindings.js, which will in turn be the case if the caller
  // of the create method provided an onEvent closure.
  if (src_id_ < 0) {
    delete data;
    return;
  }

  DictionaryValue* event = CreateAPIResourceEvent(
      API_RESOURCE_EVENT_DATA_READ);
  event->SetInteger(kResultCodeKey, result_code);
  event->Set(kDataKey, data);
  event->SetString(kAddressKey, address);
  event->SetInteger(kPortKey, port);
  DispatchEvent(events::kExperimentalSocketOnEvent, event);
}

void APIResourceEventNotifier::OnWriteComplete(int result_code) {
  SendEventWithResultCode(events::kExperimentalSocketOnEvent,
                          API_RESOURCE_EVENT_WRITE_COMPLETE, result_code);
}

void APIResourceEventNotifier::OnTransferComplete(int result_code,
                                                  base::BinaryValue* data) {
  if (src_id_ < 0) {
    delete data;
    return;
  }

  DictionaryValue* event = CreateAPIResourceEvent(
      API_RESOURCE_EVENT_TRANSFER_COMPLETE);
  event->SetInteger(kResultCodeKey, result_code);
  event->Set(kDataKey, data);
  DispatchEvent(events::kExperimentalUsbOnEvent, event);
}

// static
std::string APIResourceEventNotifier::APIResourceEventTypeToString(
    APIResourceEventType event_type) {
  switch (event_type) {
    case API_RESOURCE_EVENT_CONNECT_COMPLETE:
      return kEventTypeConnectComplete;
    case API_RESOURCE_EVENT_DATA_READ:
      return kEventTypeDataRead;
    case API_RESOURCE_EVENT_WRITE_COMPLETE:
      return kEventTypeWriteComplete;
    case API_RESOURCE_EVENT_TRANSFER_COMPLETE:
      return kEventTypeTransferComplete;
  }

  NOTREACHED();
  return std::string();
}

APIResourceEventNotifier::~APIResourceEventNotifier() {}

void APIResourceEventNotifier::DispatchEvent(const std::string &extension,
                                             DictionaryValue* event) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &APIResourceEventNotifier::DispatchEventOnUIThread, this, extension,
          event));
}

void APIResourceEventNotifier::DispatchEventOnUIThread(
    const std::string &extension, DictionaryValue* event) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ListValue args;
  args.Set(0, event);
  router_->DispatchEventToExtension(src_extension_id_, extension, args,
                                    profile_, src_url_);
}

DictionaryValue* APIResourceEventNotifier::CreateAPIResourceEvent(
    APIResourceEventType event_type) {
  DictionaryValue* event = new DictionaryValue();
  event->SetString(kEventTypeKey, APIResourceEventTypeToString(event_type));
  event->SetInteger(kSrcIdKey, src_id_);

  // TODO(miket): Signal that it's OK to clean up onEvent listeners. This is
  // the framework we'll use, but we need to start using it.
  event->SetBoolean(kIsFinalEventKey, false);

  // The caller owns the created event, which typically is then given to a
  // ListValue to dispose of.
  return event;
}

void APIResourceEventNotifier::SendEventWithResultCode(
    const std::string &extension,
    APIResourceEventType event_type,
    int result_code) {
  if (src_id_ < 0)
    return;

  DictionaryValue* event = CreateAPIResourceEvent(event_type);
  event->SetInteger(kResultCodeKey, result_code);
  DispatchEvent(extension, event);
}

}  // namespace extensions
