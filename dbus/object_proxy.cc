// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/scoped_dbus_error.h"

namespace dbus {

ObjectProxy::ObjectProxy(Bus* bus,
                         const std::string& service_name,
                         const std::string& object_path)
    : bus_(bus),
      service_name_(service_name),
      object_path_(object_path) {
}

ObjectProxy::~ObjectProxy() {
}

// Originally we tried to make |method_call| a const reference, but we
// gave up as dbus_connection_send_with_reply_and_block() takes a
// non-const pointer of DBusMessage as the second parameter.
bool ObjectProxy::CallMethodAndBlock(MethodCall* method_call,
                                     int timeout_ms,
                                     Response* response) {
  bus_->AssertOnDBusThread();

  if (!bus_->Connect())
    return false;

  method_call->SetDestination(service_name_);
  method_call->SetPath(object_path_);
  DBusMessage* request_message = method_call->raw_message();

  ScopedDBusError error;

  // Send the message synchronously.
  DBusMessage* response_message =
      bus_->SendWithReplyAndBlock(request_message, timeout_ms, error.get());

  if (!response_message) {
    LOG(ERROR) << "Failed to call method: "
               << (error.is_set() ? error.message() : "");
    return false;
  }
  response->reset_raw_message(response_message);

  return true;
}

void ObjectProxy::CallMethod(MethodCall* method_call,
                             int timeout_ms,
                             ResponseCallback callback) {
  bus_->AssertOnOriginThread();

  method_call->SetDestination(service_name_);
  method_call->SetPath(object_path_);
  // Increment the reference count so we can safely reference the
  // underlying request message until the method call is complete. This
  // will be unref'ed in StartAsyncMethodCall().
  DBusMessage* request_message = method_call->raw_message();
  dbus_message_ref(request_message);

  // Bind() won't compile if we pass request_message as-is since
  // DBusMessage is an opaque struct which Bind() cannot handle.
  // Hence we cast it to void* to workaround the issue.
  base::Closure task = base::Bind(&ObjectProxy::StartAsyncMethodCall,
                                  this,
                                  timeout_ms,
                                  static_cast<void*>(request_message),
                                  callback);
  // Wait for the response in the D-Bus thread.
  bus_->PostTaskToDBusThread(FROM_HERE, task);
}

ObjectProxy::OnPendingCallIsCompleteData::OnPendingCallIsCompleteData(
    ObjectProxy* in_object_proxy,
    ResponseCallback in_response_callback)
    : object_proxy(in_object_proxy),
      response_callback(in_response_callback) {
}

ObjectProxy::OnPendingCallIsCompleteData::~OnPendingCallIsCompleteData() {
}

void ObjectProxy::StartAsyncMethodCall(int timeout_ms,
                                       void* in_request_message,
                                       ResponseCallback response_callback) {
  bus_->AssertOnDBusThread();

  if (!bus_->Connect() || !bus_->SetUpAsyncOperations()) {
    // In case of a failure, run the callback with NULL response, that
    // indicates a failure.
    Response* response = NULL;
    base::Closure task = base::Bind(&ObjectProxy::RunResponseCallback,
                                    this,
                                    response_callback,
                                    response);
    bus_->PostTaskToOriginThread(FROM_HERE, task);
    return;
  }

  DBusMessage* request_message =
      static_cast<DBusMessage*>(in_request_message);
  DBusPendingCall* pending_call = NULL;

  bus_->SendWithReply(request_message, &pending_call, timeout_ms);

  // Prepare the data we'll be passing to OnPendingCallIsCompleteThunk().
  // The data will be deleted in OnPendingCallIsCompleteThunk().
  OnPendingCallIsCompleteData* data =
      new OnPendingCallIsCompleteData(this, response_callback);

  // This returns false only when unable to allocate memory.
  const bool success = dbus_pending_call_set_notify(
      pending_call,
      &ObjectProxy::OnPendingCallIsCompleteThunk,
      data,
      NULL);
  CHECK(success) << "Unable to allocate memory";
  dbus_pending_call_unref(pending_call);

  // It's now safe to unref the request message.
  dbus_message_unref(request_message);
}

void ObjectProxy::OnPendingCallIsComplete(DBusPendingCall* pending_call,
                                          ResponseCallback response_callback) {
  bus_->AssertOnDBusThread();

  DBusMessage* response_message = dbus_pending_call_steal_reply(pending_call);

  if (!response_message) {
    // This shouldn't happen but just in case.
    LOG(ERROR) << "The response message is not received for some reason";
    Response* response = NULL;
    base::Closure task = base::Bind(&ObjectProxy::RunResponseCallback,
                                    this,
                                    response_callback,
                                    response);
    bus_->PostTaskToOriginThread(FROM_HERE, task);
    return;
  }

  // The response message will be deleted in RunResponseCallback().
  Response* response = new Response;
  response->reset_raw_message(response_message);
  base::Closure task = base::Bind(&ObjectProxy::RunResponseCallback,
                                  this,
                                  response_callback,
                                  response);
  bus_->PostTaskToOriginThread(FROM_HERE, task);
}

void ObjectProxy::RunResponseCallback(ResponseCallback response_callback,
                                      Response* response) {
  bus_->AssertOnOriginThread();

  if (!response) {
    // The response is not received.
    response_callback.Run(NULL);
  } else if (response->GetMessageType() == Message::MESSAGE_ERROR) {
    // Error message may contain the error message as string.
    dbus::MessageReader reader(response);
    std::string error_message;
    reader.PopString(&error_message);
    LOG(ERROR) << "Failed to call method: " << response->GetErrorName()
               << ": " << error_message;
    // We don't give the error message to the callback.
    response_callback.Run(NULL);
  } else {
    // The response is successfuly received.
    response_callback.Run(response);
  }
  delete response;  // It's ok to delete NULL.
}

void ObjectProxy::OnPendingCallIsCompleteThunk(DBusPendingCall* pending_call,
                                               void* user_data) {
  OnPendingCallIsCompleteData* data =
      reinterpret_cast<OnPendingCallIsCompleteData*>(user_data);
  ObjectProxy* self = data->object_proxy;
  self->OnPendingCallIsComplete(pending_call,
                                data->response_callback);
  delete data;
}

}  // namespace dbus
