// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_IBUS_BRIDGE_H_
#define CHROMEOS_IME_IBUS_BRIDGE_H_

#include <string>
#include "base/basictypes.h"
#include "base/memory/singleton.h"

namespace chromeos {
class IBusInputContextHandlerInterface;
class IBusEngineHandlerInterface;

namespace ibus {
class IBusPanelCandidateWindowHandlerInterface;
class IBusPanelPropertyHandlerInterface;
}  // namespace ibus

// IBusBridge provides access of each IME related handler. This class is used
// for IME implementation without ibus-daemon. The legacy ibus IME communicates
// their engine with dbus protocol, but new implementation doesn't. Instead of
// dbus communcation, new implementation calls target service(e.g. PanelService
// or EngineService) directly by using this class. IBusBridge is managed as
// singleton object.
class IBusBridge {
 public:
  // Returns IBusBridge instance.
  static IBusBridge* GetInstance();

  // Returns current InputContextHandler. This function returns NULL if input
  // context is not ready to use.
  IBusInputContextHandlerInterface* input_context_handler() const {
    return input_context_handler_;
  }

  // Updates current InputContextHandler. If there is no active input context,
  // pass NULL for |handler|. Caller must release |handler|.
  void set_input_context_handler(IBusInputContextHandlerInterface* handler) {
    input_context_handler_ = handler;
  }

  // Returns current EngineHandler. This function returns NULL if current engine
  // is not ready to use.
  IBusEngineHandlerInterface* engine_handler() const {
    return engine_handler_;
  }

  // Updates current EngineHandler. If there is no active engine service, pass
  // NULL for |handler|. Caller must release |handler|.
  void set_engine_handler(IBusEngineHandlerInterface* handler) {
    engine_handler_ = handler;
  }

  // Returns current CandidateWindowHandler. This function returns NULL if
  // current candidate window is not ready to use.
  ibus::IBusPanelCandidateWindowHandlerInterface*
      candidate_window_handler() const {
    return candidate_window_handler_;
  }

  // Updates current CandidatWindowHandler. If there is no active candidate
  // window service, pass NULL for |handler|. Caller must release |handler|.
  void set_candidate_window_handler(
      ibus::IBusPanelCandidateWindowHandlerInterface* handler) {
    candidate_window_handler_ = handler;
  }

  // Returns current PropertyHandler. This function returns NULL if panel window
  // is not ready to use.
  ibus::IBusPanelPropertyHandlerInterface* panel_handler() const {
    return panel_handler_;
  }

  // Updates current PropertyHandler. If there is no active property service,
  // pass NULL for |handler|. Caller must release |handler|.
  void set_panel_handler(ibus::IBusPanelPropertyHandlerInterface* handler) {
    panel_handler_ = handler;
  }

 private:
  // Singleton implementation. Use GetInstance for getting instance.
  IBusBridge();
  friend struct DefaultSingletonTraits<IBusBridge>;

  IBusInputContextHandlerInterface* input_context_handler_;
  IBusEngineHandlerInterface* engine_handler_;
  ibus::IBusPanelCandidateWindowHandlerInterface* candidate_window_handler_;
  ibus::IBusPanelPropertyHandlerInterface* panel_handler_;

  DISALLOW_COPY_AND_ASSIGN(IBusBridge);
};

}  // namespace chromeos

#endif  // CHROMEOS_IME_IBUS_BRIDGE_H_
