// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/ibus_bridge.h"

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace chromeos {

static IBusBridge* g_ibus_bridge = NULL;

// An implementation of IBusBridge.
class IBusBridgeImpl : public IBusBridge {
 public:
  IBusBridgeImpl()
    : input_context_handler_(NULL),
      engine_handler_(NULL),
      candidate_window_handler_(NULL),
      panel_handler_(NULL) {
  }

  virtual ~IBusBridgeImpl() {
  }

  // IBusBridge override.
  virtual IBusInputContextHandlerInterface*
      GetInputContextHandler() const OVERRIDE {
    return input_context_handler_;
  }

  // IBusBridge override.
  virtual void SetInputContextHandler(
      IBusInputContextHandlerInterface* handler) OVERRIDE {
    input_context_handler_ = handler;
  }

  // IBusBridge override.
  virtual IBusEngineHandlerInterface* GetEngineHandler() const OVERRIDE {
    return engine_handler_;
  }

  // IBusBridge override.
  virtual void SetEngineHandler(IBusEngineHandlerInterface* handler) OVERRIDE {
    engine_handler_ = handler;
  }

  // IBusBridge override.
  virtual IBusPanelCandidateWindowHandlerInterface*
  GetCandidateWindowHandler() const OVERRIDE {
    return candidate_window_handler_;
  }

  // IBusBridge override.
  virtual void SetCandidateWindowHandler(
      IBusPanelCandidateWindowHandlerInterface* handler) OVERRIDE {
    candidate_window_handler_ = handler;
  }

  // IBusBridge override.
  virtual IBusPanelPropertyHandlerInterface* GetPanelHandler() const OVERRIDE {
    return panel_handler_;
  }

  // IBusBridge override.
  virtual void SetPanelHandler(
      IBusPanelPropertyHandlerInterface* handler) OVERRIDE {
    panel_handler_ = handler;
  }

 private:
  IBusInputContextHandlerInterface* input_context_handler_;
  IBusEngineHandlerInterface* engine_handler_;
  IBusPanelCandidateWindowHandlerInterface* candidate_window_handler_;
  IBusPanelPropertyHandlerInterface* panel_handler_;

  DISALLOW_COPY_AND_ASSIGN(IBusBridgeImpl);
};

///////////////////////////////////////////////////////////////////////////////
// IBusBridge
IBusBridge::IBusBridge() {
}

IBusBridge::~IBusBridge() {
}

// static.
void IBusBridge::Initialize() {
  CHECK(!g_ibus_bridge) << "Already initialized.";
  g_ibus_bridge = new IBusBridgeImpl();
}

// static.
void IBusBridge::Shutdown() {
  CHECK(g_ibus_bridge) << "Shutdown called before Initialize().";
  delete g_ibus_bridge;
  g_ibus_bridge = NULL;
}

// static.
IBusBridge* IBusBridge::Get() {
  return g_ibus_bridge;
}

}  // namespace chromeos
