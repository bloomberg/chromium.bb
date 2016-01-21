// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_IME_ARC_IME_IPC_HOST_IMPL_H_
#define COMPONENTS_ARC_IME_ARC_IME_IPC_HOST_IMPL_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/ime.mojom.h"
#include "components/arc/ime/arc_ime_ipc_host.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
struct CompositionText;
}  // namespace ui

namespace arc {

// This class encapsulates the detail of IME related IPC between
// Chromium and the ARC container.
class ArcImeIpcHostImpl : public ArcImeIpcHost,
                          public ImeHost,
                          public ArcBridgeService::Observer {
 public:
  ArcImeIpcHostImpl(Delegate* delegate, ArcBridgeService* bridge_service);
  ~ArcImeIpcHostImpl() override;

  // arc::ArcBridgeService::Observer overrides:
  void OnImeInstanceReady() override;

  // ArcImeHost overrides:
  void SendSetCompositionText(const ui::CompositionText& composition) override;
  void SendConfirmCompositionText() override;
  void SendInsertText(const base::string16& text) override;

  // arc::ImeHost overrides:
  void OnTextInputTypeChanged(arc::TextInputType type) override;
  void OnCursorRectChanged(arc::CursorRectPtr rect) override;

 private:
  mojo::Binding<ImeHost> binding_;
  Delegate* const delegate_;
  ArcBridgeService* const bridge_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcImeIpcHostImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_IME_ARC_IME_IPC_HOST_IMPL_H_
