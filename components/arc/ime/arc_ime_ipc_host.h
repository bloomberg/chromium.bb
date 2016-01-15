// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_IME_ARC_IME_IPC_HOST_H_
#define COMPONENTS_ARC_IME_ARC_IME_IPC_HOST_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/ime.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
struct CompositionText;
}  // namespace ui

namespace arc {

// This class encapsulates the detail of IME related IPC between
// Chromium and the ARC container.
class ArcImeIpcHost : public ImeHost,
                      public ArcBridgeService::Observer {
 public:
  // Received IPCs are deserialized and passed to this delegate.
  class Delegate {
   public:
    virtual void OnTextInputTypeChanged(ui::TextInputType type) = 0;
    virtual void OnCursorRectChanged(const gfx::Rect& rect) = 0;
  };

  ArcImeIpcHost(Delegate* delegate, ArcBridgeService* bridge_service);
  ~ArcImeIpcHost() override;

  // arc::ArcBridgeService::Observer:
  void OnImeInstanceReady() override;

  // Serializes and sends IME related requests through IPCs.
  void SendSetCompositionText(const ui::CompositionText& composition);
  void SendConfirmCompositionText();
  void SendInsertText(const base::string16& text);

  // arc::ImeHost:
  void OnTextInputTypeChanged(arc::TextInputType type) override;
  void OnCursorRectChanged(arc::CursorRectPtr rect) override;

 private:
  mojo::Binding<ImeHost> binding_;
  Delegate* const delegate_;
  ArcBridgeService* const bridge_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcImeIpcHost);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_IME_ARC_IME_IPC_HOST_H_
