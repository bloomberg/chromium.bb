// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/exported_object.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/scoped_dbus_error.h"

namespace dbus {

namespace {

// Gets the absolute method name by concatenating the interface name and
// the method name. Used for building keys for method_table_ in
// ExportedObject.
std::string GetAbsoluteMethodName(
    const std::string& interface_name,
    const std::string& method_name) {
  return interface_name + "." + method_name;
}

}  // namespace

ExportedObject::ExportedObject(Bus* bus,
                               const std::string& service_name,
                               const std::string& object_path)
    : bus_(bus),
      service_name_(service_name),
      object_path_(object_path),
      object_is_registered_(false),
      method_is_called_(false),
      response_from_method_(NULL),
      on_method_is_called_(&method_is_called_lock_) {
}

ExportedObject::~ExportedObject() {
  DCHECK(!object_is_registered_);
}

bool ExportedObject::ExportMethodAndBlock(
    const std::string& interface_name,
    const std::string& method_name,
    MethodCallCallback method_call_callback) {
  bus_->AssertOnDBusThread();

  if (!bus_->Connect())
    return false;
  if (!bus_->SetUpAsyncOperations())
    return false;
  if (!bus_->RequestOwnership(service_name_))
    return false;
  if (!Register())
    return false;

  const std::string absolute_method_name =
      GetAbsoluteMethodName(interface_name, method_name);
  if (method_table_.find(absolute_method_name) != method_table_.end()) {
    LOG(ERROR) << absolute_method_name << " is already exported";
    return false;
  }
  method_table_[absolute_method_name] = method_call_callback;

  return true;
}

void ExportedObject::ExportMethod(const std::string& interface_name,
                                  const std::string& method_name,
                                  MethodCallCallback method_call_callback,
                                  OnExportedCallback on_exported_calback) {
  bus_->AssertOnOriginThread();

  base::Closure task = base::Bind(&ExportedObject::ExportMethodInternal,
                                  this,
                                  interface_name,
                                  method_name,
                                  method_call_callback,
                                  on_exported_calback);
  bus_->PostTaskToDBusThread(FROM_HERE, task);
}

void ExportedObject::Unregister() {
  bus_->AssertOnDBusThread();

  if (!object_is_registered_)
    return;

  bus_->UnregisterObjectPath(object_path_);
  object_is_registered_ = false;
}

void ExportedObject::ExportMethodInternal(
    const std::string& interface_name,
    const std::string& method_name,
    MethodCallCallback method_call_callback,
    OnExportedCallback on_exported_calback) {
  bus_->AssertOnDBusThread();

  const bool success = ExportMethodAndBlock(interface_name,
                                            method_name,
                                            method_call_callback);
  bus_->PostTaskToOriginThread(FROM_HERE,
                               base::Bind(&ExportedObject::OnExported,
                                          this,
                                          on_exported_calback,
                                          interface_name,
                                          method_name,
                                          success));
}

void ExportedObject::OnExported(OnExportedCallback on_exported_callback,
                                const std::string& interface_name,
                                const std::string& method_name,
                                bool success) {
  bus_->AssertOnOriginThread();

  on_exported_callback.Run(interface_name, method_name, success);
}

bool ExportedObject::Register() {
  bus_->AssertOnDBusThread();

  if (object_is_registered_)
    return true;

  ScopedDBusError error;

  DBusObjectPathVTable vtable = {};
  vtable.message_function = &ExportedObject::HandleMessageThunk;
  vtable.unregister_function = &ExportedObject::OnUnregisteredThunk;
  const bool success = bus_->TryRegisterObjectPath(object_path_,
                                                   &vtable,
                                                   this,
                                                   error.get());
  if (!success) {
    LOG(ERROR) << "Failed to regiser the object: " << object_path_ << ": "
               << (error.is_set() ? error.message() : "");
    return false;
  }

  object_is_registered_ = true;
  return true;
}

DBusHandlerResult ExportedObject::HandleMessage(
    DBusConnection* connection,
    DBusMessage* raw_message) {
  bus_->AssertOnDBusThread();
  DCHECK_EQ(DBUS_MESSAGE_TYPE_METHOD_CALL, dbus_message_get_type(raw_message));

  // raw_message will be unrefed on exit of the function. Increment the
  // reference so we can use it in MethodCall.
  dbus_message_ref(raw_message);
  scoped_ptr<MethodCall> method_call(
      MethodCall::FromRawMessage(raw_message));
  const std::string interface = method_call->GetInterface();
  const std::string member = method_call->GetMember();

  if (interface.empty()) {
    // We don't support method calls without interface.
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  // Check if we know about the method.
  const std::string absolute_method_name = GetAbsoluteMethodName(
      interface, member);
  MethodTable::const_iterator iter = method_table_.find(absolute_method_name);
  if (iter == method_table_.end()) {
    // Don't know about the method.
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  Response* response = NULL;
  if (bus_->HasDBusThread()) {
    response_from_method_ = NULL;
    method_is_called_ = false;
    // Post a task to run the method in the origin thread.
    bus_->PostTaskToOriginThread(FROM_HERE,
                                 base::Bind(&ExportedObject::RunMethod,
                                            this,
                                            iter->second,
                                            method_call.get()));
    // Wait until the method call is done. Blocking is not desirable but we
    // should return the response to the dbus-daemon in the function, so we
    // don't have a choice. We wait in the D-Bus thread, so it should be ok.
    {
      // We need a timeout here in case the method gets stuck.
      const int kTimeoutSecs = 10;
      const base::TimeDelta timeout(
          base::TimeDelta::FromSeconds(kTimeoutSecs));
      const base::Time start_time = base::Time::Now();

      base::AutoLock auto_lock(method_is_called_lock_);
      while (!method_is_called_) {
        on_method_is_called_.TimedWait(timeout);
        CHECK(base::Time::Now() - start_time < timeout)
            << "Method " << absolute_method_name << " timed out";
      }
    }
    response = response_from_method_;
  } else {
    // If the D-Bus thread is not used, just call the method directly. We
    // don't need the complicated logic to wait for the method call to be
    // complete.
    response = iter->second.Run(method_call.get());
  }

  if (!response) {
    // Something bad happend in the method call.
    scoped_ptr<dbus::ErrorResponse> error_response(
        ErrorResponse::FromMethodCall(method_call.get(),
                                      DBUS_ERROR_FAILED,
                                      "error occurred in " + member));
    dbus_connection_send(connection, error_response->raw_message(), NULL);
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  // The method call was successful.
  dbus_connection_send(connection, response->raw_message(), NULL);
  delete response;

  return DBUS_HANDLER_RESULT_HANDLED;
}

void ExportedObject::RunMethod(MethodCallCallback method_call_callback,
                               MethodCall* method_call) {
  bus_->AssertOnOriginThread();

  base::AutoLock auto_lock(method_is_called_lock_);
  response_from_method_ = method_call_callback.Run(method_call);
  method_is_called_ = true;
  on_method_is_called_.Signal();
}

void ExportedObject::OnUnregistered(DBusConnection* connection) {
}

DBusHandlerResult ExportedObject::HandleMessageThunk(
    DBusConnection* connection,
    DBusMessage* raw_message,
    void* user_data) {
  ExportedObject* self = reinterpret_cast<ExportedObject*>(user_data);
  return self->HandleMessage(connection, raw_message);
}

void ExportedObject::OnUnregisteredThunk(DBusConnection *connection,
                                         void* user_data) {
  ExportedObject* self = reinterpret_cast<ExportedObject*>(user_data);
  return self->OnUnregistered(connection);
}

}  // namespace dbus
