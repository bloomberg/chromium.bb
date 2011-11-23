// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/scoped_dbus_error.h"

namespace {

// Used for success ratio histograms. 1 for success, 0 for failure.
const int kSuccessRatioHistogramMaxValue = 2;

// Gets the absolute signal name by concatenating the interface name and
// the signal name. Used for building keys for method_table_ in
// ObjectProxy.
std::string GetAbsoluteSignalName(
    const std::string& interface_name,
    const std::string& signal_name) {
  return interface_name + "." + signal_name;
}

// An empty function used for ObjectProxy::EmptyResponseCallback().
void EmptyResponseCallbackBody(dbus::Response* unused_response) {
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
  const base::TimeTicks start_time = base::TimeTicks::Now();
  DBusMessage* response_message =
      bus_->SendWithReplyAndBlock(request_message, timeout_ms, error.get());
  // Record if the method call is successful, or not. 1 if successful.
  UMA_HISTOGRAM_ENUMERATION("DBus.SyncMethodCallSuccess",
                            response_message ? 1 : 0,
                            kSuccessRatioHistogramMaxValue);

  if (!response_message) {
    LOG(ERROR) << "Failed to call method: "
               << (error.is_set() ? error.message() : "");
    return NULL;
  }
  // Record time spent for the method call. Don't include failures.
  UMA_HISTOGRAM_TIMES("DBus.SyncMethodCallTime",
                      base::TimeTicks::Now() - start_time);

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

  const base::TimeTicks start_time = base::TimeTicks::Now();
  base::Closure task = base::Bind(&ObjectProxy::StartAsyncMethodCall,
                                  this,
                                  timeout_ms,
                                  request_message,
                                  callback,
                                  start_time);
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

  if (filter_added_) {
    if (!bus_->RemoveFilterFunction(&ObjectProxy::HandleMessageThunk, this)) {
      LOG(ERROR) << "Failed to remove filter function";
    }
  }

  for (std::set<std::string>::iterator iter = match_rules_.begin();
       iter != match_rules_.end(); ++iter) {
    ScopedDBusError error;
    bus_->RemoveMatch(*iter, error.get());
    if (error.is_set()) {
      // There is nothing we can do to recover, so just print the error.
      LOG(ERROR) << "Failed to remove match rule: " << *iter;
    }
  }
  match_rules_.clear();
}

// static
ObjectProxy::ResponseCallback ObjectProxy::EmptyResponseCallback() {
  return base::Bind(&EmptyResponseCallbackBody);
}

ObjectProxy::OnPendingCallIsCompleteData::OnPendingCallIsCompleteData(
    ObjectProxy* in_object_proxy,
    ResponseCallback in_response_callback,
    base::TimeTicks in_start_time)
    : object_proxy(in_object_proxy),
      response_callback(in_response_callback),
      start_time(in_start_time) {
}

ObjectProxy::OnPendingCallIsCompleteData::~OnPendingCallIsCompleteData() {
}

void ObjectProxy::StartAsyncMethodCall(int timeout_ms,
                                       DBusMessage* request_message,
                                       ResponseCallback response_callback,
                                       base::TimeTicks start_time) {
  bus_->AssertOnDBusThread();

  if (!bus_->Connect() || !bus_->SetUpAsyncOperations()) {
    // In case of a failure, run the callback with NULL response, that
    // indicates a failure.
    DBusMessage* response_message = NULL;
    base::Closure task = base::Bind(&ObjectProxy::RunResponseCallback,
                                    this,
                                    response_callback,
                                    start_time,
                                    response_message);
    bus_->PostTaskToOriginThread(FROM_HERE, task);
    return;
  }

  DBusPendingCall* pending_call = NULL;

  bus_->SendWithReply(request_message, &pending_call, timeout_ms);

  // Prepare the data we'll be passing to OnPendingCallIsCompleteThunk().
  // The data will be deleted in OnPendingCallIsCompleteThunk().
  OnPendingCallIsCompleteData* data =
      new OnPendingCallIsCompleteData(this, response_callback, start_time);

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
                                          ResponseCallback response_callback,
                                          base::TimeTicks start_time) {
  bus_->AssertOnDBusThread();

  DBusMessage* response_message = dbus_pending_call_steal_reply(pending_call);
  base::Closure task = base::Bind(&ObjectProxy::RunResponseCallback,
                                  this,
                                  response_callback,
                                  start_time,
                                  response_message);
  bus_->PostTaskToOriginThread(FROM_HERE, task);
}

void ObjectProxy::RunResponseCallback(ResponseCallback response_callback,
                                      base::TimeTicks start_time,
                                      DBusMessage* response_message) {
  bus_->AssertOnOriginThread();

  bool method_call_successful = false;
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
    // The response is successfully received.
    response_callback.Run(response.get());
    method_call_successful = true;
    // Record time spent for the method call. Don't include failures.
    UMA_HISTOGRAM_TIMES("DBus.AsyncMethodCallTime",
                        base::TimeTicks::Now() - start_time);
  }
  // Record if the method call is successful, or not. 1 if successful.
  UMA_HISTOGRAM_ENUMERATION("DBus.AsyncMethodCallSuccess",
                            method_call_successful,
                            kSuccessRatioHistogramMaxValue);
}

void ObjectProxy::OnPendingCallIsCompleteThunk(DBusPendingCall* pending_call,
                                               void* user_data) {
  OnPendingCallIsCompleteData* data =
      reinterpret_cast<OnPendingCallIsCompleteData*>(user_data);
  ObjectProxy* self = data->object_proxy;
  self->OnPendingCallIsComplete(pending_call,
                                data->response_callback,
                                data->start_time);
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
      if (bus_->AddFilterFunction(&ObjectProxy::HandleMessageThunk, this)) {
        filter_added_ = true;
      } else {
        LOG(ERROR) << "Failed to add filter function";
      }
    }
    // Add a match rule so the signal goes through HandleMessage().
    //
    // We don't restrict the sender object path to be |object_path_| here,
    // to make it easy to test D-Bus signal handling with dbus-send, that
    // uses "/" as the sender object path. We can make the object path
    // restriction customizable when it becomes necessary.
    const std::string match_rule =
        base::StringPrintf("type='signal', interface='%s'",
                           interface_name.c_str());

    // Add the match rule if we don't have it.
    if (match_rules_.find(match_rule) == match_rules_.end()) {
      ScopedDBusError error;
      bus_->AddMatch(match_rule, error.get());;
      if (error.is_set()) {
        LOG(ERROR) << "Failed to add match rule: " << match_rule;
      } else {
        // Store the match rule, so that we can remove this in Detach().
        match_rules_.insert(match_rule);
        // Add the signal callback to the method table.
        method_table_[absolute_signal_name] = signal_callback;
        success = true;
      }
    } else {
      // We already have the match rule.
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

  const std::string interface = signal->GetInterface();
  const std::string member = signal->GetMember();

  // Check if we know about the signal.
  const std::string absolute_signal_name = GetAbsoluteSignalName(
      interface, member);
  MethodTable::const_iterator iter = method_table_.find(absolute_signal_name);
  if (iter == method_table_.end()) {
    // Don't know about the signal.
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  VLOG(1) << "Signal received: " << signal->ToString();

  const base::TimeTicks start_time = base::TimeTicks::Now();
  if (bus_->HasDBusThread()) {
    // Post a task to run the method in the origin thread.
    // Transfer the ownership of |signal| to RunMethod().
    // |released_signal| will be deleted in RunMethod().
    Signal* released_signal = signal.release();
    bus_->PostTaskToOriginThread(FROM_HERE,
                                 base::Bind(&ObjectProxy::RunMethod,
                                            this,
                                            start_time,
                                            iter->second,
                                            released_signal));
  } else {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    // If the D-Bus thread is not used, just call the callback on the
    // current thread. Transfer the ownership of |signal| to RunMethod().
    Signal* released_signal = signal.release();
    RunMethod(start_time, iter->second, released_signal);
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

void ObjectProxy::RunMethod(base::TimeTicks start_time,
                            SignalCallback signal_callback,
                            Signal* signal) {
  bus_->AssertOnOriginThread();

  signal_callback.Run(signal);
  delete signal;
  // Record time spent for handling the signal.
  UMA_HISTOGRAM_TIMES("DBus.SignalHandleTime",
                      base::TimeTicks::Now() - start_time);
}

DBusHandlerResult ObjectProxy::HandleMessageThunk(
    DBusConnection* connection,
    DBusMessage* raw_message,
    void* user_data) {
  ObjectProxy* self = reinterpret_cast<ObjectProxy*>(user_data);
  return self->HandleMessage(connection, raw_message);
}

}  // namespace dbus
