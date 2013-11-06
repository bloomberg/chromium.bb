// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_component.h"
#include "chromeos/dbus/ibus/ibus_engine_service.h"
#include "chromeos/ime/ibus_bridge.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

// An implementation of IBusClient without ibus-daemon interaction.
// Currently this class is used only on linux desktop.
// TODO(nona): Use this on ChromeOS device once crbug.com/171351 is fixed.
class IBusClientDaemonlessImpl : public IBusClient {
 public:
  IBusClientDaemonlessImpl() {}
  virtual ~IBusClientDaemonlessImpl() {}

  virtual void CreateInputContext(
      const std::string& client_name,
      const CreateInputContextCallback & callback,
      const ErrorCallback& error_callback) OVERRIDE {
    // TODO(nona): Remove this function once ibus-daemon is gone.
    // We don't have to do anything for this function except for calling
    // |callback| as the success of this function. The original spec of ibus
    // supports multiple input contexts, but there is only one input context in
    // Chrome OS. That is all IME events will be came from same input context
    // and all engine action shuold be forwarded to same input context.
    dbus::ObjectPath path("dummpy path");
    callback.Run(path);
  }

  virtual void RegisterComponent(
      const IBusComponent& ibus_component,
      const RegisterComponentCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    // TODO(nona): Remove this function once ibus-daemon is gone.
    // The information about engine is stored in chromeos::InputMethodManager so
    // IBusBridge doesn't do anything except for calling |callback| as success
    // of this function.
    callback.Run();
  }

  virtual void SetGlobalEngine(const std::string& engine_name,
                               const ErrorCallback& error_callback) OVERRIDE {
    IBusEngineHandlerInterface* previous_engine =
        IBusBridge::Get()->GetEngineHandler();
    if (previous_engine)
      previous_engine->Disable();
    IBusBridge::Get()->CreateEngine(engine_name);
    IBusEngineHandlerInterface* next_engine =
        IBusBridge::Get()->GetEngineHandler();
    IBusBridge::Get()->SetEngineHandler(next_engine);
    if (next_engine)
      next_engine->Enable();
  }

  virtual void Exit(ExitOption option,
                    const ErrorCallback& error_callback) OVERRIDE {
    // Exit is not supported on daemon-less implementation.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusClientDaemonlessImpl);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IBusClient

IBusClient::IBusClient() {}

IBusClient::~IBusClient() {}

// static
IBusClient* IBusClient::Create() {
  return new IBusClientDaemonlessImpl();
}

}  // namespace chromeos
