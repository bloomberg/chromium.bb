// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_IBUS_BRIDGE_H_
#define CHROMEOS_IME_IBUS_BRIDGE_H_

#include <string>
#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/ibus/ibus_engine_factory_service.h"

namespace chromeos {
class IBusInputContextHandlerInterface;
class IBusEngineHandlerInterface;
class IBusPanelCandidateWindowHandlerInterface;
class IBusPanelPropertyHandlerInterface;

// IBusBridge provides access of each IME related handler. This class is used
// for IME implementation without ibus-daemon. The legacy ibus IME communicates
// their engine with dbus protocol, but new implementation doesn't. Instead of
// dbus communcation, new implementation calls target service(e.g. PanelService
// or EngineService) directly by using this class.
class IBusBridge {
 public:
  virtual ~IBusBridge();

  // Allocates the global instance. Must be called before any calls to Get().
  static CHROMEOS_EXPORT void Initialize();

  // Releases the global instance.
  static CHROMEOS_EXPORT void Shutdown();

  // Returns IBusBridge global instance. Initialize() must be called first.
  static CHROMEOS_EXPORT IBusBridge* Get();

  // Returns current InputContextHandler. This function returns NULL if input
  // context is not ready to use.
  virtual IBusInputContextHandlerInterface* GetInputContextHandler() const = 0;

  // Updates current InputContextHandler. If there is no active input context,
  // pass NULL for |handler|. Caller must release |handler|.
  virtual void SetInputContextHandler(
      IBusInputContextHandlerInterface* handler) = 0;

  // Returns current EngineHandler. This function returns NULL if current engine
  // is not ready to use.
  virtual IBusEngineHandlerInterface* GetEngineHandler() const = 0;

  // Updates current EngineHandler. If there is no active engine service, pass
  // NULL for |handler|. Caller must release |handler|.
  virtual void SetEngineHandler(IBusEngineHandlerInterface* handler) = 0;

  // Returns current CandidateWindowHandler. This function returns NULL if
  // current candidate window is not ready to use.
  virtual IBusPanelCandidateWindowHandlerInterface*
      GetCandidateWindowHandler() const = 0;

  // Updates current CandidatWindowHandler. If there is no active candidate
  // window service, pass NULL for |handler|. Caller must release |handler|.
  virtual void SetCandidateWindowHandler(
      IBusPanelCandidateWindowHandlerInterface* handler) = 0;

  // Returns current PropertyHandler. This function returns NULL if panel window
  // is not ready to use.
  virtual IBusPanelPropertyHandlerInterface* GetPropertyHandler() const = 0;

  // Updates current PropertyHandler. If there is no active property service,
  // pass NULL for |handler|. Caller must release |handler|.
  virtual void SetPropertyHandler(
      IBusPanelPropertyHandlerInterface* handler) = 0;

  // Sets create engine handler for |engine_id|. |engine_id| must not be empty
  // and |handler| must not be null.
  virtual void SetCreateEngineHandler(
      const std::string& engine_id,
      const IBusEngineFactoryService::CreateEngineHandler& handler) = 0;

  // Unsets create engine handler for |engine_id|. |engine_id| must not be
  // empty.
  virtual void UnsetCreateEngineHandler(const std::string& engine_id) = 0;

  // Creates engine. Do not call this function before SetCreateEngineHandler
  // call with |engine_id|.
  virtual void CreateEngine(const std::string& engine_id) = 0;

 protected:
  IBusBridge();

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusBridge);
};

}  // namespace chromeos

#endif  // CHROMEOS_IME_IBUS_BRIDGE_H_
