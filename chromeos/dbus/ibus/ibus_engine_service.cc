// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_engine_service.h"

#include <string>
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_lookup_table.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

class IBusEngineServiceImpl : public IBusEngineService {
 public:
  IBusEngineServiceImpl(dbus::Bus* bus,
                        const dbus::ObjectPath& object_path)
      : bus_(bus),
        engine_handler_(NULL),
        object_path_(object_path),
        weak_ptr_factory_(this) {
    exported_object_ = bus->GetExportedObject(object_path_);

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kFocusInMethod,
        base::Bind(&IBusEngineServiceImpl::FocusIn,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kFocusOutMethod,
        base::Bind(&IBusEngineServiceImpl::FocusOut,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kEnableMethod,
        base::Bind(&IBusEngineServiceImpl::Enable,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kDisableMethod,
        base::Bind(&IBusEngineServiceImpl::Disable,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kPropertyActivateMethod,
        base::Bind(&IBusEngineServiceImpl::PropertyActivate,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kPropertyShowMethod,
        base::Bind(&IBusEngineServiceImpl::PropertyShow,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kPropertyHideMethod,
        base::Bind(&IBusEngineServiceImpl::PropertyHide,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kSetCapabilityMethod,
        base::Bind(&IBusEngineServiceImpl::SetCapability,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kResetMethod,
        base::Bind(&IBusEngineServiceImpl::Reset,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kProcessKeyEventMethod,
        base::Bind(&IBusEngineServiceImpl::ProcessKeyEvent,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kCandidateClickedMethod,
        base::Bind(&IBusEngineServiceImpl::CandidateClicked,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kSetSurroundingTextMethod,
        base::Bind(&IBusEngineServiceImpl::SetSurroundingText,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusEngineServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~IBusEngineServiceImpl() {
    bus_->UnregisterExportedObject(object_path_);
  }

  // IBusEngineService override.
  virtual void SetEngine(IBusEngineHandlerInterface* handler) OVERRIDE {
    DVLOG_IF(1, engine_handler_ != NULL) << "Replace engine.";
    engine_handler_ = handler;
  }

  // IBusEngineService override.
  virtual void UnsetEngine() OVERRIDE {
    LOG_IF(ERROR, engine_handler_ == NULL) << "There is no engine.";
    engine_handler_ = NULL;
  }

  // IBusEngineService override.
  virtual void RegisterProperties(
      const IBusPropertyList& property_list) OVERRIDE {
    dbus::Signal signal(ibus::engine::kServiceInterface,
                        ibus::engine::kRegisterPropertiesSignal);
    dbus::MessageWriter writer(&signal);
    AppendIBusPropertyList(property_list, &writer);
    exported_object_->SendSignal(&signal);
  }

  // IBusEngineService override.
  virtual void UpdatePreedit(const IBusText& ibus_text,
                             uint32 cursor_pos,
                             bool is_visible,
                             IBusEnginePreeditFocusOutMode mode) OVERRIDE {
    dbus::Signal signal(ibus::engine::kServiceInterface,
                        ibus::engine::kUpdatePreeditSignal);
    dbus::MessageWriter writer(&signal);
    AppendIBusText(ibus_text, &writer);
    writer.AppendUint32(cursor_pos);
    writer.AppendBool(is_visible);
    writer.AppendUint32(static_cast<uint32>(mode));
    exported_object_->SendSignal(&signal);
  }

  // IBusEngineService override.
  virtual void UpdateAuxiliaryText(const IBusText& ibus_text,
                                   bool is_visible) OVERRIDE {
    dbus::Signal signal(ibus::engine::kServiceInterface,
                        ibus::engine::kUpdateAuxiliaryTextSignal);
    dbus::MessageWriter writer(&signal);
    AppendIBusText(ibus_text, &writer);
    writer.AppendBool(is_visible);
    exported_object_->SendSignal(&signal);
  }

  // IBusEngineService override.
  virtual void UpdateLookupTable(const IBusLookupTable& lookup_table,
                                 bool is_visible) OVERRIDE {
    dbus::Signal signal(ibus::engine::kServiceInterface,
                        ibus::engine::kUpdateLookupTableSignal);
    dbus::MessageWriter writer(&signal);
    AppendIBusLookupTable(lookup_table, &writer);
    writer.AppendBool(is_visible);
    exported_object_->SendSignal(&signal);
  }

  // IBusEngineService override.
  virtual void UpdateProperty(const IBusProperty& property) OVERRIDE {
    dbus::Signal signal(ibus::engine::kServiceInterface,
                        ibus::engine::kUpdatePropertySignal);
    dbus::MessageWriter writer(&signal);
    AppendIBusProperty(property, &writer);
    exported_object_->SendSignal(&signal);
  }

  // IBusEngineService override.
  virtual void ForwardKeyEvent(uint32 keyval, uint32 keycode,
                               uint32 state) OVERRIDE {
    dbus::Signal signal(ibus::engine::kServiceInterface,
                        ibus::engine::kForwardKeyEventSignal);
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(keyval);
    writer.AppendUint32(keycode);
    writer.AppendUint32(state);
    exported_object_->SendSignal(&signal);
  }

  // IBusEngineService override.
  virtual void RequireSurroundingText() OVERRIDE {
    dbus::Signal signal(ibus::engine::kServiceInterface,
                        ibus::engine::kRequireSurroundingTextSignal);
    exported_object_->SendSignal(&signal);
  }

  virtual void CommitText(const std::string& text) OVERRIDE {
    dbus::Signal signal(ibus::engine::kServiceInterface,
                        ibus::engine::kCommitTextSignal);
    dbus::MessageWriter writer(&signal);
    AppendStringAsIBusText(text, &writer);
    exported_object_->SendSignal(&signal);
  }

 private:
  // Handles FocusIn method call from ibus-daemon.
  void FocusIn(dbus::MethodCall* method_call,
               dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    engine_handler_->FocusIn();
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Handles FocusOut method call from ibus-daemon.
  void FocusOut(dbus::MethodCall* method_call,
                dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    engine_handler_->FocusOut();
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Handles Enable method call from ibus-daemon.
  void Enable(dbus::MethodCall* method_call,
              dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    engine_handler_->Enable();
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Handles Disable method call from ibus-daemon.
  void Disable(dbus::MethodCall* method_call,
               dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    engine_handler_->Disable();
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Handles PropertyActivate method call from ibus-daemon.
  void PropertyActivate(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    dbus::MessageReader reader(method_call);
    std::string property_name;
    if (!reader.PopString(&property_name)) {
      LOG(WARNING) << "PropertyActivate called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    uint32 property_state = 0;
    if (!reader.PopUint32(&property_state)) {
      LOG(WARNING) << "PropertyActivate called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    engine_handler_->PropertyActivate(
        property_name,
        static_cast<ibus::IBusPropertyState>(property_state));
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Handles PropertyShow method call from ibus-daemon.
  void PropertyShow(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    dbus::MessageReader reader(method_call);
    std::string property_name;
    if (!reader.PopString(&property_name)) {
      LOG(WARNING) << "PropertyShow called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    engine_handler_->PropertyShow(property_name);
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Handles PropertyHide method call from ibus-daemon.
  void PropertyHide(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    dbus::MessageReader reader(method_call);
    std::string property_name;
    if (!reader.PopString(&property_name)) {
      LOG(WARNING) << "PropertyHide called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    engine_handler_->PropertyHide(property_name);
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Handles SetCapability method call from ibus-daemon.
  void SetCapability(dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    dbus::MessageReader reader(method_call);
    uint32 capability = 0;
    if (!reader.PopUint32(&capability)) {
      LOG(WARNING) << "SetCapability called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    engine_handler_->SetCapability(
        static_cast<IBusEngineHandlerInterface::IBusCapability>(capability));
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  void Reset(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    engine_handler_->Reset();
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Handles ProcessKeyEvent method call from ibus-daemon.
  void ProcessKeyEvent(dbus::MethodCall* method_call,
                       dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    dbus::MessageReader reader(method_call);
    uint32 keysym = 0;
    if (!reader.PopUint32(&keysym)) {
      LOG(WARNING) << "ProcessKeyEvent called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    uint32 keycode = 0;
    if (!reader.PopUint32(&keycode)) {
      LOG(WARNING) << "ProcessKeyEvent called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    uint32 state = 0;
    if (!reader.PopUint32(&state)) {
      LOG(WARNING) << "ProcessKeyEvent called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    engine_handler_->ProcessKeyEvent(
        keysym, keycode, state,
        base::Bind(&IBusEngineServiceImpl::KeyEventDone,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(dbus::Response::FromMethodCall(method_call)),
                   response_sender));
  }

  void KeyEventDone(scoped_ptr<dbus::Response> response,
                    const dbus::ExportedObject::ResponseSender& response_sender,
                    bool consume) {
    if (engine_handler_ == NULL)
      return;
    dbus::MessageWriter writer(response.get());
    writer.AppendBool(consume);
    response_sender.Run(response.Pass());
  }

  // Handles CandidateClicked method call from ibus-daemon.
  void CandidateClicked(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    dbus::MessageReader reader(method_call);
    uint32 index = 0;
    if (!reader.PopUint32(&index)) {
      LOG(WARNING) << "CandidateClicked called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    uint32 button = 0;
    if (!reader.PopUint32(&button)) {
      LOG(WARNING) << "CandidateClicked called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    uint32 state = 0;
    if (!reader.PopUint32(&state)) {
      LOG(WARNING) << "CandidateClicked called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    engine_handler_->CandidateClicked(
        index,
        static_cast<ibus::IBusMouseButton>(button),
        state);
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Handles SetSurroundingText method call from ibus-daemon.
  void SetSurroundingText(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender) {
    if (engine_handler_ == NULL)
      return;
    dbus::MessageReader reader(method_call);
    std::string text;
    if (!PopStringFromIBusText(&reader, &text)) {
      LOG(WARNING) << "SetSurroundingText called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    uint32 cursor_pos = 0;
    if (!reader.PopUint32(&cursor_pos)) {
      LOG(WARNING) << "CandidateClicked called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    uint32 anchor_pos = 0;
    if (!reader.PopUint32(&anchor_pos)) {
      LOG(WARNING) << "CandidateClicked called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }

    engine_handler_->SetSurroundingText(text, cursor_pos, anchor_pos);
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Called when the method call is exported.
  void OnMethodExported(const std::string& interface_name,
                        const std::string& method_name,
                        bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // D-Bus bus object used for unregistering exported methods in dtor.
  dbus::Bus* bus_;

  // All incoming method calls are passed on to the |engine_handler_|.
  IBusEngineHandlerInterface* engine_handler_;

  dbus::ObjectPath object_path_;
  scoped_refptr<dbus::ExportedObject> exported_object_;
  base::WeakPtrFactory<IBusEngineServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusEngineServiceImpl);
};

// An implementation of IBusEngineService without ibus-daemon interaction.
// Currently this class is used only on linux desktop.
// TODO(nona): Use this on ChromeOS device once crbug.com/171351 is fixed.
class IBusEngineServiceDaemonlessImpl : public IBusEngineService {
 public:
  IBusEngineServiceDaemonlessImpl() {}
  virtual ~IBusEngineServiceDaemonlessImpl() {}

  // IBusEngineService override.
  virtual void SetEngine(IBusEngineHandlerInterface* handler) OVERRIDE {
    // TODO(nona): Implement this.
  }

  // IBusEngineService override.
  virtual void UnsetEngine() OVERRIDE {
    // TODO(nona): Implement this.
  }

  // IBusEngineService override.
  virtual void RegisterProperties(
      const IBusPropertyList& property_list) OVERRIDE {
    // TODO(nona): Implement this.
  }

  // IBusEngineService override.
  virtual void UpdatePreedit(const IBusText& ibus_text,
                             uint32 cursor_pos,
                             bool is_visible,
                             IBusEnginePreeditFocusOutMode mode) OVERRIDE {
    // TODO(nona): Implement this.
  }

  // IBusEngineService override.
  virtual void UpdateAuxiliaryText(const IBusText& ibus_text,
                                   bool is_visible) OVERRIDE {
    // TODO(nona): Implement this.
  }

  // IBusEngineService override.
  virtual void UpdateLookupTable(const IBusLookupTable& lookup_table,
                                 bool is_visible) OVERRIDE {
    // TODO(nona): Implement this.
  }

  // IBusEngineService override.
  virtual void UpdateProperty(const IBusProperty& property) OVERRIDE {
    // TODO(nona): Implement this.
  }

  // IBusEngineService override.
  virtual void ForwardKeyEvent(uint32 keyval, uint32 keycode,
                               uint32 state) OVERRIDE {
    // TODO(nona): Implement this.
  }

  // IBusEngineService override.
  virtual void RequireSurroundingText() OVERRIDE {
    // TODO(nona): Implement this.
  }

  // IBusEngineService override.
  virtual void CommitText(const std::string& text) OVERRIDE {
    // TODO(nona): Implement this.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusEngineServiceDaemonlessImpl);
};

IBusEngineService::IBusEngineService() {
}

IBusEngineService::~IBusEngineService() {
}

// static
IBusEngineService* IBusEngineService::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new IBusEngineServiceImpl(bus, object_path);
  else
    return new IBusEngineServiceDaemonlessImpl();
}

}  // namespace chromeos
