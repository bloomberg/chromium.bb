// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_input_context_client.h"

#include <string>
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// TODO(nona): Remove after complete libibus removal.
using chromeos::ibus::IBusText;

namespace {

// The IBusInputContextClient implementation.
class IBusInputContextClientImpl : public IBusInputContextClient {
 public:
  IBusInputContextClientImpl()
      : proxy_(NULL),
        weak_ptr_factory_(this) {
  }

  virtual ~IBusInputContextClientImpl() {}

 public:
  // IBusInputContextClient override.
  virtual void Initialize(dbus::Bus* bus,
                          const dbus::ObjectPath& object_path) OVERRIDE {
    if (proxy_ != NULL) {
      LOG(ERROR) << "IBusInputContextClient is already initialized.";
      return;
    }
    proxy_ = bus->GetObjectProxy(ibus::kServiceName, object_path);

    ConnectSignals();
  }

  // IBusInputContextClient override.
  virtual void SetInputContextHandler(
      IBusInputContextHandlerInterface* handler) OVERRIDE {
    handler_ = handler;
  }

  // IBusInputContextClient override.
  virtual void ResetObjectProxy() OVERRIDE {
    // Do not delete proxy here, proxy object is managed by dbus::Bus object.
    proxy_ = NULL;
  }

  // IBusInputContextClient override.
  virtual bool IsObjectProxyReady() const OVERRIDE {
    return proxy_ != NULL;
  }

  // IBusInputContextClient override.
  virtual void SetCapabilities(uint32 capabilities) OVERRIDE {
    dbus::MethodCall method_call(ibus::input_context::kServiceInterface,
                                 ibus::input_context::kSetCapabilitiesMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(capabilities);
    CallNoResponseMethod(&method_call,
                         ibus::input_context::kSetCapabilitiesMethod);
  }

  // IBusInputContextClient override.
  virtual void FocusIn() OVERRIDE {
    dbus::MethodCall method_call(ibus::input_context::kServiceInterface,
                                 ibus::input_context::kFocusInMethod);
    CallNoResponseMethod(&method_call, ibus::input_context::kFocusInMethod);
  }

  // IBusInputContextClient override.
  virtual void FocusOut() OVERRIDE {
    dbus::MethodCall method_call(ibus::input_context::kServiceInterface,
                                 ibus::input_context::kFocusOutMethod);
    CallNoResponseMethod(&method_call, ibus::input_context::kFocusOutMethod);
  }

  // IBusInputContextClient override.
  virtual void Reset() OVERRIDE {
    dbus::MethodCall method_call(ibus::input_context::kServiceInterface,
                                 ibus::input_context::kResetMethod);
    CallNoResponseMethod(&method_call, ibus::input_context::kResetMethod);
  }

  // IBusInputContextClient override.
  virtual void SetCursorLocation(int32 x, int32 y, int32 width,
                                 int32 height) OVERRIDE {
    dbus::MethodCall method_call(ibus::input_context::kServiceInterface,
                                 ibus::input_context::kSetCursorLocationMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(x);
    writer.AppendInt32(y);
    writer.AppendInt32(width);
    writer.AppendInt32(height);
    CallNoResponseMethod(&method_call,
                         ibus::input_context::kSetCursorLocationMethod);
  }

  // IBusInputContextClient override.
  virtual void ProcessKeyEvent(
      uint32 keyval,
      uint32 keycode,
      uint32 state,
      const ProcessKeyEventCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(ibus::input_context::kServiceInterface,
                                 ibus::input_context::kProcessKeyEventMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(keyval);
    writer.AppendUint32(keycode);
    writer.AppendUint32(state);
    proxy_->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&IBusInputContextClientImpl::OnProcessKeyEvent,
                   callback,
                   error_callback),
        base::Bind(&IBusInputContextClientImpl::OnProecessKeyEventFail,
                   error_callback));
  }

  // IBusInputContextClient override.
  virtual void SetSurroundingText(const std::string& text,
                                  uint32 start_index,
                                  uint32 end_index) OVERRIDE {
    dbus::MethodCall method_call(
        ibus::input_context::kServiceInterface,
        ibus::input_context::kSetSurroundingTextMethod);
    dbus::MessageWriter writer(&method_call);
    ibus::AppendStringAsIBusText(text, &writer);
    writer.AppendUint32(start_index);
    writer.AppendUint32(end_index);
    CallNoResponseMethod(&method_call,
                         ibus::input_context::kSetSurroundingTextMethod);
  }

  // IBusInputContextClient override.
  virtual void PropertyActivate(const std::string& key,
                                ibus::IBusPropertyState state) OVERRIDE {
    dbus::MethodCall method_call(ibus::input_context::kServiceInterface,
                                 ibus::input_context::kPropertyActivateMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(key);
    if (state == ibus::IBUS_PROPERTY_STATE_CHECKED) {
      writer.AppendUint32(ibus::IBUS_PROPERTY_STATE_CHECKED);
    } else {
      writer.AppendUint32(ibus::IBUS_PROPERTY_STATE_UNCHECKED);
    }
    CallNoResponseMethod(&method_call,
                         ibus::input_context::kPropertyActivateMethod);
  }
 private:
  void CallNoResponseMethod(dbus::MethodCall* method_call,
                            const std::string& method_name) {
    proxy_->CallMethodWithErrorCallback(
        method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&IBusInputContextClientImpl::DefaultCallback,
                   method_name),
        base::Bind(&IBusInputContextClientImpl::DefaultErrorCallback,
                   method_name));
  }

  // Handles no response method call reply.
  static void DefaultCallback(const std::string& method_name,
                              dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to call method: " << method_name;
      return;
    }
  }

  // Handles error response of default method call.
  static void DefaultErrorCallback(const std::string& method_name,
                                   dbus::ErrorResponse* response) {
    LOG(ERROR) << "Failed to call method: " << method_name;
  }

  // Handles ProcessKeyEvent method call reply.
  static void OnProcessKeyEvent(const ProcessKeyEventCallback& callback,
                                const ErrorCallback& error_callback,
                                dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Cannot get input context: " << response->ToString();
      error_callback.Run();
      return;
    }
    dbus::MessageReader reader(response);
    bool is_keyevent_used;
    if (!reader.PopBool(&is_keyevent_used)) {
      // The IBus message structure may be changed.
      LOG(ERROR) << "Invalid response: " << response->ToString();
      error_callback.Run();
      return;
    }
    DCHECK(!callback.is_null());
    callback.Run(is_keyevent_used);
  }

  // Handles error response of ProcessKeyEvent method call.
  static void OnProecessKeyEventFail(const ErrorCallback& error_callback,
                                     dbus::ErrorResponse* response) {
    error_callback.Run();
  }

  // Handles CommitText signal.
  void OnCommitText(dbus::Signal* signal) {
    if (!handler_)
      return;
    dbus::MessageReader reader(signal);
    IBusText ibus_text;
    if (!PopIBusText(&reader, &ibus_text)) {
      // The IBus message structure may be changed.
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    handler_->CommitText(ibus_text);
  }

  // Handles ForwardKeyEvetn signal.
  void OnForwardKeyEvent(dbus::Signal* signal) {
    if (!handler_)
      return;
    dbus::MessageReader reader(signal);
    uint32 keyval = 0;
    uint32 keycode = 0;
    uint32 state = 0;
    if (!reader.PopUint32(&keyval) ||
        !reader.PopUint32(&keycode) ||
        !reader.PopUint32(&state)) {
      // The IBus message structure may be changed.
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    handler_->ForwardKeyEvent(keyval, keycode, state);
  }

  // Handles UpdatePreeditText signal.
  void OnUpdatePreeditText(dbus::Signal* signal) {
    if (!handler_)
      return;
    dbus::MessageReader reader(signal);
    IBusText ibus_text;
    uint32 cursor_pos = 0;
    bool visible = true;
    if (!PopIBusText(&reader, &ibus_text) ||
        !reader.PopUint32(&cursor_pos) ||
        !reader.PopBool(&visible)) {
      // The IBus message structure may be changed.
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    handler_->UpdatePreeditText(ibus_text, cursor_pos, visible);
  }

  // Handles ShowPreeditText signal.
  void OnShowPreeditText(dbus::Signal* signal) {
    if (handler_)
      handler_->ShowPreeditText();
  }

  // Handles HidePreeditText signal.
  void OnHidePreeditText(dbus::Signal* signal) {
    if (handler_)
      handler_->HidePreeditText();
  }

  // Connects signals to signal handlers.
  void ConnectSignals() {
    proxy_->ConnectToSignal(
        ibus::input_context::kServiceInterface,
        ibus::input_context::kCommitTextSignal,
        base::Bind(&IBusInputContextClientImpl::OnCommitText,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusInputContextClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    proxy_->ConnectToSignal(
        ibus::input_context::kServiceInterface,
        ibus::input_context::kForwardKeyEventSignal,
        base::Bind(&IBusInputContextClientImpl::OnForwardKeyEvent,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusInputContextClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    proxy_->ConnectToSignal(
        ibus::input_context::kServiceInterface,
        ibus::input_context::kUpdatePreeditTextSignal,
        base::Bind(&IBusInputContextClientImpl::OnUpdatePreeditText,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusInputContextClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    proxy_->ConnectToSignal(
        ibus::input_context::kServiceInterface,
        ibus::input_context::kShowPreeditTextSignal,
        base::Bind(&IBusInputContextClientImpl::OnShowPreeditText,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusInputContextClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    proxy_->ConnectToSignal(
        ibus::input_context::kServiceInterface,
        ibus::input_context::kHidePreeditTextSignal,
        base::Bind(&IBusInputContextClientImpl::OnHidePreeditText,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusInputContextClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded) << "Connect to " << interface << " "
                              << signal << " failed.";
  }

  dbus::ObjectProxy* proxy_;

  // The pointer for input context handler. This can be NULL.
  IBusInputContextHandlerInterface* handler_;

  base::WeakPtrFactory<IBusInputContextClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusInputContextClientImpl);
};

// A stub implementation of IBusInputContextClient.
class IBusInputContextClientStubImpl : public IBusInputContextClient {
 public:
  IBusInputContextClientStubImpl() {}

  virtual ~IBusInputContextClientStubImpl() {}

 public:
  // IBusInputContextClient override.
  virtual void Initialize(dbus::Bus* bus,
                          const dbus::ObjectPath& object_path) OVERRIDE {}
  virtual void SetInputContextHandler(
      IBusInputContextHandlerInterface* handler) OVERRIDE {}
  // IBusInputContextClient override.
  virtual void ResetObjectProxy() OVERRIDE {}
  // IBusInputContextClient override.
  virtual bool IsObjectProxyReady() const OVERRIDE {
    return true;
  }
  // IBusInputContextClient overrides.
  virtual void SetCapabilities(uint32 capability) OVERRIDE {}
  virtual void FocusIn() OVERRIDE {}
  virtual void FocusOut() OVERRIDE {}
  virtual void Reset() OVERRIDE {}
  virtual void SetCursorLocation(int32 x, int32 y, int32 w, int32 h) OVERRIDE {}
  virtual void ProcessKeyEvent(
      uint32 keyval,
      uint32 keycode,
      uint32 state,
      const ProcessKeyEventCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    callback.Run(false);
  }
  virtual void SetSurroundingText(const std::string& text,
                                  uint32 start_index,
                                  uint32 end_index) OVERRIDE {}
  virtual void PropertyActivate(const std::string& key,
                                ibus::IBusPropertyState state) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusInputContextClientStubImpl);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IBusInputContextClient

IBusInputContextClient::IBusInputContextClient() {}

IBusInputContextClient::~IBusInputContextClient() {}

// static
IBusInputContextClient* IBusInputContextClient::Create(
    DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION) {
    return new IBusInputContextClientImpl();
  }
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new IBusInputContextClientStubImpl();
}
}  // namespace chromeos
