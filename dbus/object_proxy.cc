// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/scoped_dbus_error.h"

namespace {

// Gets the absolute signal name by concatenating the interface name and
// the signal name. Used for building keys for method_table_ in
// ObjectProxy.
std::string GetAbsoluteSignalName(
    const std::string& interface_name,
    const std::string& signal_name) {
  return interface_name + "." + signal_name;
}

}  // namespace

namespace dbus {

ObjectProxy::ObjectProxy(Bus* bus,
                         const std::string& service_name,
                         const std::string& object_path)
    : bus_(bus),
      service_name_(service_name),
      object_path_(object_path),
      filter_added_(false) {
}

ObjectProxy::~ObjectProxy() {
}

// Originally we tried to make |method_call| a const reference, but we
// gave up as dbus_connection_send_with_reply_and_block() takes a
// non-const pointer of DBusMessage as the second parameter.
Response* ObjectProxy::CallMethodAndBlock(MethodCall* method_call,
                                          int timeout_ms) {
  bus_->AssertOnDBusThread();

  if (!bus_->Connect())
    return NULL;

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
    return NULL;
  }

  return Response::FromRawMessage(response_message);
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

void ObjectProxy::ConnectToSignal(const std::string& interface_name,
                                  const std::string& signal_name,
                                  SignalCallback signal_callback,
                                  OnConnectedCallback on_connected_callback) {
  bus_->AssertOnOriginThread();

  bus_->PostTaskToDBusThread(FROM_HERE,
                             base::Bind(&ObjectProxy::ConnectToSignalInternal,
                                        this,
                                        interface_name,
                                        signal_name,
                                        signal_callback,
                                        on_connected_callback));
}

void ObjectProxy::Detach() {
  bus_->AssertOnDBusThread();

  if (filter_added_)
    bus_->RemoveFilterFunction(&ObjectProxy::HandleMessageThunk, this);

  for (size_t i = 0; i < match_rules_.size(); ++i) {
    ScopedDBusError error;
    bus_->RemoveMatch(match_rules_[i], error.get());
    if (error.is_set()) {
      // There is nothing we can do to recover, so just print the error.
      LOG(ERROR) << "Failed to remove match rule: " << match_rules_[i];
    }
  }
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
  // |response_message| will be unref'ed in RunResponseCallback().
  // Bind() won't compile if we pass response_message as-is.
  // See CallMethod() for details.
  base::Closure task = base::Bind(&ObjectProxy::RunResponseCallback,
                                  this,
                                  response_callback,
                                  static_cast<void*>(response_message));
  bus_->PostTaskToOriginThread(FROM_HERE, task);
}

void ObjectProxy::RunResponseCallback(ResponseCallback response_callback,
                                      void* in_response_message) {
  bus_->AssertOnOriginThread();
  DBusMessage* response_message =
      static_cast<DBusMessage*>(in_response_message);

  if (!response_message) {
    // The response is not received.
    response_callback.Run(NULL);
  } else if (dbus_message_get_type(response_message) ==
             DBUS_MESSAGE_TYPE_ERROR) {
    // This will take |response_message| and release (unref) it.
    scoped_ptr<dbus::ErrorResponse> error_response(
        dbus::ErrorResponse::FromRawMessage(response_message));
    // Error message may contain the error message as string.
    dbus::MessageReader reader(error_response.get());
    std::string error_message;
    reader.PopString(&error_message);
    LOG(ERROR) << "Failed to call method: " << error_response->GetErrorName()
               << ": " << error_message;
    // We don't give the error message to the callback.
    response_callback.Run(NULL);
  } else {
    // This will take |response_message| and release (unref) it.
    scoped_ptr<dbus::Response> response(
        dbus::Response::FromRawMessage(response_message));
    // The response is successfuly received.
    response_callback.Run(response.get());
  }
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

void ObjectProxy::ConnectToSignalInternal(
    const std::string& interface_name,
    const std::string& signal_name,
    SignalCallback signal_callback,
    OnConnectedCallback on_connected_callback) {
  bus_->AssertOnDBusThread();

  // Check if the object is already connected to the signal.
  const std::string absolute_signal_name =
      GetAbsoluteSignalName(interface_name, signal_name);
  if (method_table_.find(absolute_signal_name) != method_table_.end()) {
    LOG(ERROR) << "The object proxy is already connected to "
               << absolute_signal_name;
    return;
  }

  // Will become true, if everything is successful.
  bool success = false;

  if (bus_->Connect() && bus_->SetUpAsyncOperations()) {
    // We should add the filter only once. Otherwise, HandleMessage() will
    // be called more than once.
    if (!filter_added_) {
      bus_->AddFilterFunction(&ObjectProxy::HandleMessageThunk, this);
      filter_added_ = true;
    }
    // Add a match rule so the signal goes through HandleMessage().
    const std::string match_rule =
        base::StringPrintf("type='signal', interface='%s', path='%s'",
                           interface_name.c_str(),
                           object_path_.c_str());
    ScopedDBusError error;
    bus_->AddMatch(match_rule, error.get());;
    if (error.is_set()) {
      LOG(ERROR) << "Failed to add match rule: " << match_rule;
    } else {
      // Store the match rule, so that we can remove this in Detach().
      match_rules_.push_back(match_rule);
      // Add the signal callback to the method table.
      method_table_[absolute_signal_name] = signal_callback;
      success = true;
    }
  }

  // Run on_connected_callback in the origin thread.
  bus_->PostTaskToOriginThread(
      FROM_HERE,
      base::Bind(&ObjectProxy::OnConnected,
                 this,
                 on_connected_callback,
                 interface_name,
                 signal_name,
                 success));
}

void ObjectProxy::OnConnected(OnConnectedCallback on_connected_callback,
                              const std::string& interface_name,
                              const std::string& signal_name,
                              bool success) {
  bus_->AssertOnOriginThread();

  on_connected_callback.Run(interface_name, signal_name, success);
}

DBusHandlerResult ObjectProxy::HandleMessage(
    DBusConnection* connection,
    DBusMessage* raw_message) {
  bus_->AssertOnDBusThread();
  if (dbus_message_get_type(raw_message) != DBUS_MESSAGE_TYPE_SIGNAL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  // raw_message will be unrefed on exit of the function. Increment the
  // reference so we can use it in Signal.
  dbus_message_ref(raw_message);
  scoped_ptr<Signal> signal(
      Signal::FromRawMessage(raw_message));

  // The signal is not coming from the remote object we are attaching to.
  if (signal->GetPath() != object_path_)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  const std::string interface = signal->GetInterface();
  const std::string member = signal->GetMember();

  // Check if we know about the method.
  const std::string absolute_signal_name = GetAbsoluteSignalName(
      interface, member);
  MethodTable::const_iterator iter = method_table_.find(absolute_signal_name);
  if (iter == method_table_.end()) {
    // Don't know about the method.
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  if (bus_->HasDBusThread()) {
    // Post a task to run the method in the origin thread.
    // Transfer the ownership of |signal| to RunMethod().
    // |released_signal| will be deleted in RunMethod().
    Signal* released_signal = signal.release();
    bus_->PostTaskToOriginThread(FROM_HERE,
                                 base::Bind(&ObjectProxy::RunMethod,
                                            this,
                                            iter->second,
                                            released_signal));
  } else {
    // If the D-Bus thread is not used, just call the method directly. We
    // don't need the complicated logic to wait for the method call to be
    // complete.
    iter->second.Run(signal.get());
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

void ObjectProxy::RunMethod(SignalCallback signal_callback,
                            Signal* signal) {
  bus_->AssertOnOriginThread();

  signal_callback.Run(signal);
  delete signal;
}

DBusHandlerResult ObjectProxy::HandleMessageThunk(
    DBusConnection* connection,
    DBusMessage* raw_message,
    void* user_data) {
  ObjectProxy* self = reinterpret_cast<ObjectProxy*>(user_data);
  return self->HandleMessage(connection, raw_message);
}

}  // namespace dbus
