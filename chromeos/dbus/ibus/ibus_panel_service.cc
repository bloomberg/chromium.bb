// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_panel_service.h"

#include <string>
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_input_context_client.h"
#include "chromeos/dbus/ibus/ibus_lookup_table.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {
// TODO(nona): Remove ibus namespace after complete libibus removal.
namespace ibus {

class IBusPanelServiceImpl : public IBusPanelService {
 public:
  explicit IBusPanelServiceImpl(dbus::Bus* bus,
                                IBusInputContextClient* input_context)
      : bus_(bus),
        candidate_window_handler_(NULL),
        property_handler_(NULL),
        weak_ptr_factory_(this) {
    exported_object_ = bus->GetExportedObject(
        dbus::ObjectPath(ibus::panel::kServicePath));

    exported_object_->ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kUpdateLookupTableMethod,
        base::Bind(&IBusPanelServiceImpl::UpdateLookupTable,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusPanelServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kHideLookupTableMethod,
        base::Bind(&IBusPanelServiceImpl::HideLookupTable,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusPanelServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kUpdateAuxiliaryTextMethod,
        base::Bind(&IBusPanelServiceImpl::UpdateAuxiliaryText,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusPanelServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kHideAuxiliaryTextMethod,
        base::Bind(&IBusPanelServiceImpl::HideAuxiliaryText,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusPanelServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kUpdatePreeditTextMethod,
        base::Bind(&IBusPanelServiceImpl::UpdatePreeditText,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusPanelServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kHidePreeditTextMethod,
        base::Bind(&IBusPanelServiceImpl::HidePreeditText,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusPanelServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kRegisterPropertiesMethod,
        base::Bind(&IBusPanelServiceImpl::RegisterProperties,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusPanelServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kUpdatePropertyMethod,
        base::Bind(&IBusPanelServiceImpl::UpdateProperty,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusPanelServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kFocusInMethod,
        base::Bind(&IBusPanelServiceImpl::NoOperation,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusPanelServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kFocusOutMethod,
        base::Bind(&IBusPanelServiceImpl::NoOperation,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusPanelServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kStateChangedMethod,
        base::Bind(&IBusPanelServiceImpl::NoOperation,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&IBusPanelServiceImpl::OnMethodExported,
                   weak_ptr_factory_.GetWeakPtr()));

    // Request well known name to ibus-daemon.
    bus->RequestOwnership(
        ibus::panel::kServiceName,
        base::Bind(&IBusPanelServiceImpl::OnRequestOwnership,
                   weak_ptr_factory_.GetWeakPtr()));

    input_context->SetSetCursorLocationHandler(
        base::Bind(&IBusPanelServiceImpl::SetCursorLocation,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~IBusPanelServiceImpl() {
    bus_->UnregisterExportedObject(
        dbus::ObjectPath(ibus::panel::kServicePath));
  }

  // IBusPanelService override.
  virtual void SetUpCandidateWindowHandler(
      IBusPanelCandidateWindowHandlerInterface* handler) {
    DCHECK(handler);
    candidate_window_handler_ = handler;
  }

  // IBusPanelService override.
  virtual void SetUpPropertyHandler(
      IBusPanelPropertyHandlerInterface* handler) {
    DCHECK(handler);
    property_handler_ = handler;
  }

  // IBusPanelService override.
  virtual void CandidateClicked(uint32 index,
                                ibus::IBusMouseButton button,
                                uint32 state) OVERRIDE {
    dbus::Signal signal(ibus::panel::kServiceInterface,
                        ibus::panel::kCandidateClickedSignal);
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(index);
    writer.AppendUint32(static_cast<uint32>(button));
    writer.AppendUint32(state);
    exported_object_->SendSignal(&signal);
  }

  // IBusPanelService override.
  virtual void CursorUp() OVERRIDE {
    dbus::Signal signal(ibus::panel::kServiceInterface,
                        ibus::panel::kCursorUpSignal);
    exported_object_->SendSignal(&signal);
  }

  // IBusPanelService override.
  virtual void CursorDown() OVERRIDE {
    dbus::Signal signal(ibus::panel::kServiceInterface,
                        ibus::panel::kCursorDownSignal);
    exported_object_->SendSignal(&signal);
  }

  // IBusPanelService override.
  virtual void PageUp() OVERRIDE {
    dbus::Signal signal(ibus::panel::kServiceInterface,
                        ibus::panel::kPageUpSignal);
    exported_object_->SendSignal(&signal);
  }

  // IBusPanelService override.
  virtual void PageDown() OVERRIDE {
    dbus::Signal signal(ibus::panel::kServiceInterface,
                        ibus::panel::kPageDownSignal);
    exported_object_->SendSignal(&signal);
  }

 private:
  // Handles UpdateLookupTable method call from ibus-daemon.
  void UpdateLookupTable(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender) {
    if (!candidate_window_handler_)
      return;

    dbus::MessageReader reader(method_call);
    ibus::IBusLookupTable table;
    if (!ibus::PopIBusLookupTable(&reader, &table)) {
      LOG(WARNING) << "UpdateLookupTable called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    bool visible = false;
    if (!reader.PopBool(&visible)) {
      LOG(WARNING) << "UpdateLookupTable called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    candidate_window_handler_->UpdateLookupTable(table, visible);
    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Handles HideLookupTable method call from ibus-daemon.
  void HideLookupTable(dbus::MethodCall* method_call,
                       dbus::ExportedObject::ResponseSender response_sender) {
    if (!candidate_window_handler_)
      return;

    candidate_window_handler_->HideLookupTable();
    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Handles UpdateAuxiliaryText method call from ibus-daemon.
  void UpdateAuxiliaryText(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender) {
    if (!candidate_window_handler_)
      return;

    dbus::MessageReader reader(method_call);
    std::string text;
    if (!ibus::PopStringFromIBusText(&reader, &text)) {
      LOG(WARNING) << "UpdateAuxiliaryText called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    bool visible = false;
    if (!reader.PopBool(&visible)) {
      LOG(WARNING) << "UpdateAuxiliaryText called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    candidate_window_handler_->UpdateAuxiliaryText(text, visible);
    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Handles HideAuxiliaryText method call from ibus-daemon.
  void HideAuxiliaryText(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender) {
    if (!candidate_window_handler_)
      return;

    candidate_window_handler_->HideAuxiliaryText();
    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Handles UpdatePreeditText method call from ibus-daemon.
  void UpdatePreeditText(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender) {
    if (!candidate_window_handler_)
      return;

    dbus::MessageReader reader(method_call);
    std::string text;
    if (!ibus::PopStringFromIBusText(&reader, &text)) {
      LOG(WARNING) << "UpdatePreeditText called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    uint32 cursor_pos = 0;
    if (!reader.PopUint32(&cursor_pos)) {
      LOG(WARNING) << "UpdatePreeditText called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    bool visible = false;
    if (!reader.PopBool(&visible)) {
      LOG(WARNING) << "UpdatePreeditText called with incorrect parameters: "
                   << method_call->ToString();
      return;
    }
    candidate_window_handler_->UpdatePreeditText(text, cursor_pos, visible);
    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Handles HidePreeditText method call from ibus-daemon.
  void HidePreeditText(dbus::MethodCall* method_call,
                       dbus::ExportedObject::ResponseSender response_sender) {
    if (!candidate_window_handler_)
      return;

    candidate_window_handler_->HidePreeditText();
    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Handles RegisterProperties method call from ibus-daemon.
  void RegisterProperties(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender) {
    if (!property_handler_)
      return;

    dbus::MessageReader reader(method_call);
    ibus::IBusPropertyList properties;
    if (!ibus::PopIBusPropertyList(&reader, &properties)) {
      DLOG(WARNING) << "RegisterProperties called with incorrect parameters:"
                    << method_call->ToString();
      return;
    }
    property_handler_->RegisterProperties(properties);

    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Handles UpdateProperty method call from ibus-daemon.
  void UpdateProperty(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender) {
    if (!property_handler_)
      return;

    dbus::MessageReader reader(method_call);
    ibus::IBusProperty property;
    if (!ibus::PopIBusProperty(&reader, &property)) {
      DLOG(WARNING) << "RegisterProperties called with incorrect parameters:"
                    << method_call->ToString();
      return;
    }
    property_handler_->UpdateProperty(property);

    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  void SetCursorLocation(const ibus::Rect& cursor_location,
                         const ibus::Rect& composition_head) {
    // TODO(nona): implement this function.
  }

  // Handles FocusIn, FocusOut, StateChanged method calls from IBus, and ignores
  // them.
  void NoOperation(dbus::MethodCall* method_call,
                   dbus::ExportedObject::ResponseSender response_sender) {
    if (!property_handler_)
      return;

    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Called when the method call is exported.
  void OnMethodExported(const std::string& interface_name,
                        const std::string& method_name,
                        bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Called when the well knwon name is acquired.
  void OnRequestOwnership(const std::string& name, bool obtained) {
    LOG_IF(ERROR, !obtained) << "Failed to acquire well known name:"
                             << name;
  }

  // D-Bus bus object used for unregistering exported methods in dtor.
  dbus::Bus* bus_;

  // All incoming method calls are passed on to the |candidate_window_handler_|
  // or |property_handler|. This class does not take ownership of following
  // handlers.
  IBusPanelCandidateWindowHandlerInterface* candidate_window_handler_;
  IBusPanelPropertyHandlerInterface* property_handler_;

  scoped_refptr<dbus::ExportedObject> exported_object_;
  base::WeakPtrFactory<IBusPanelServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusPanelServiceImpl);
};

// An implementation of IBusPanelService without ibus-daemon interaction.
// Currently this class is used only on linux desktop.
// TODO(nona): Use this on ChromeOS device once crbug.com/171351 is fixed.
class IBusPanelServiceDaemonlessImpl : public IBusPanelService {
 public:
  IBusPanelServiceDaemonlessImpl() {}
  virtual ~IBusPanelServiceDaemonlessImpl() {}

  // IBusPanelService override.
  virtual void SetUpCandidateWindowHandler(
      IBusPanelCandidateWindowHandlerInterface* handler) {
    // TODO(nona): Implement this.
  }

  // IBusPanelService override.
  virtual void SetUpPropertyHandler(
      IBusPanelPropertyHandlerInterface* handler) {
    // TODO(nona): Implement this.
  }

  // IBusPanelService override.
  virtual void CandidateClicked(uint32 index,
                                ibus::IBusMouseButton button,
                                uint32 state) OVERRIDE {
    // TODO(nona): Implement this.
  }

  // IBusPanelService override.
  virtual void CursorUp() OVERRIDE {
    // Cursor Up is not supported on Chrome OS.
  }

  // IBusPanelService override.
  virtual void CursorDown() OVERRIDE {
    // Cursor Down is not supported on Chrome OS.
  }

  // IBusPanelService override.
  virtual void PageUp() OVERRIDE {
    // Page Up is not supported on Chrome OS.
  }

  // IBusPanelService override.
  virtual void PageDown() OVERRIDE {
    // Page Down is not supported on Chrome OS.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusPanelServiceDaemonlessImpl);
};

IBusPanelService::IBusPanelService() {
}

IBusPanelService::~IBusPanelService() {
}

// static
IBusPanelService* IBusPanelService::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus,
    IBusInputContextClient* input_context) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION) {
    return new IBusPanelServiceImpl(bus, input_context);
  } else {
    return new IBusPanelServiceDaemonlessImpl();
  }
}

}  // namespace ibus
}  // namespace chromeos
