// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_event_dispatcher.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/serial/serial_connection.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

namespace api {

namespace {

bool ShouldPauseOnReceiveError(serial::ReceiveError error) {
  return error == serial::RECEIVE_ERROR_DEVICE_LOST ||
      error == serial::RECEIVE_ERROR_SYSTEM_ERROR ||
      error == serial::RECEIVE_ERROR_DISCONNECTED;
}

}  // namespace

static base::LazyInstance<ProfileKeyedAPIFactory<SerialEventDispatcher> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<SerialEventDispatcher>*
    SerialEventDispatcher::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
SerialEventDispatcher* SerialEventDispatcher::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<SerialEventDispatcher>::GetForProfile(profile);
}

SerialEventDispatcher::SerialEventDispatcher(Profile* profile)
    : thread_id_(SerialConnection::kThreadId),
      profile_(profile) {
  ApiResourceManager<SerialConnection>* manager =
      ApiResourceManager<SerialConnection>::Get(profile);
  DCHECK(manager) << "No serial connection manager.";
  connections_ = manager->data_;
}

SerialEventDispatcher::~SerialEventDispatcher() {}

SerialEventDispatcher::ReceiveParams::ReceiveParams() {}

SerialEventDispatcher::ReceiveParams::~ReceiveParams() {}

void SerialEventDispatcher::PollConnection(const std::string& extension_id,
                                           int connection_id) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  ReceiveParams params;
  params.thread_id = thread_id_;
  params.profile_id = profile_;
  params.extension_id = extension_id;
  params.connections = connections_;
  params.connection_id = connection_id;

  StartReceive(params);
}

// static
void SerialEventDispatcher::StartReceive(const ReceiveParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  SerialConnection* connection =
      params.connections->Get(params.extension_id, params.connection_id);
  if (!connection)
    return;
  DCHECK(params.extension_id == connection->owner_extension_id());

  if (connection->paused())
    return;

  connection->Receive(base::Bind(&ReceiveCallback, params));
}

// static
void SerialEventDispatcher::ReceiveCallback(const ReceiveParams& params,
                                            const std::string& data,
                                            serial::ReceiveError error) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  // Note that an error (e.g. timeout) does not necessarily mean that no data
  // was read, so we may fire an onReceive regardless of any error code.
  if (data.length() > 0) {
    serial::ReceiveInfo receive_info;
    receive_info.connection_id = params.connection_id;
    receive_info.data = data;
    scoped_ptr<base::ListValue> args = serial::OnReceive::Create(receive_info);
    scoped_ptr<extensions::Event> event(
        new extensions::Event(serial::OnReceive::kEventName, args.Pass()));
    PostEvent(params, event.Pass());
  }

  if (error != serial::RECEIVE_ERROR_NONE) {
    serial::ReceiveErrorInfo error_info;
    error_info.connection_id = params.connection_id;
    error_info.error = error;
    scoped_ptr<base::ListValue> args =
        serial::OnReceiveError::Create(error_info);
    scoped_ptr<extensions::Event> event(
        new extensions::Event(serial::OnReceiveError::kEventName, args.Pass()));
    PostEvent(params, event.Pass());
    if (ShouldPauseOnReceiveError(error)) {
      SerialConnection* connection =
          params.connections->Get(params.extension_id, params.connection_id);
      if (connection)
        connection->set_paused(true);
    }
  }

  // Queue up the next read operation.
  BrowserThread::PostTask(params.thread_id,
                          FROM_HERE,
                          base::Bind(&StartReceive, params));
}

// static
void SerialEventDispatcher::PostEvent(const ReceiveParams& params,
                                      scoped_ptr<extensions::Event> event) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DispatchEvent,
                 params.profile_id,
                 params.extension_id,
                 base::Passed(event.Pass())));
}

// static
void SerialEventDispatcher::DispatchEvent(void* profile_id,
                                          const std::string& extension_id,
                                          scoped_ptr<extensions::Event> event) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  EventRouter* router = ExtensionSystem::Get(profile)->event_router();
  if (router)
    router->DispatchEventToExtension(extension_id, event.Pass());
}

}  // namespace api

}  // namespace extensions
