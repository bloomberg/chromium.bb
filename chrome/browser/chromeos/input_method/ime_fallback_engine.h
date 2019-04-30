// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IME_FALLBACK_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IME_FALLBACK_ENGINE_H_

#include "ash/public/interfaces/constants.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/ime/mojo/ime.mojom.h"
#include "ui/base/ime/mojo/ime_engine_factory_registry.mojom.h"

namespace chromeos {

// The fallback ImeEngine with empty implementation, which is used when there
// is no real engine available.
// The ProcessKeyEvent() method makes sure the callback is called so that the
// client can continue processing the key events.
class ImeFallbackEngine : public ime::mojom::ImeEngine,
                          public ime::mojom::ImeEngineFactory {
 public:
  ImeFallbackEngine();
  ~ImeFallbackEngine() override;

  void Activate();

  // ime::mojom::ImeEngineFactory overrides:
  void CreateEngine(ime::mojom::ImeEngineRequest engine_request,
                    ime::mojom::ImeEngineClientPtr client) override;

  // ime::mojom::ImeEngine overrides:
  void StartInput(ime::mojom::EditorInfoPtr info) override {}
  void FinishInput() override {}
  void CancelInput() override {}
  void ProcessKeyEvent(
      std::unique_ptr<ui::Event> key_event,
      ime::mojom::ImeEngine::ProcessKeyEventCallback cb) override;
  void UpdateSurroundingInfo(const std::string& text,
                             int32_t cursor,
                             int32_t anchor,
                             int32_t offset) override {}
  void UpdateCompositionBounds(const std::vector<gfx::Rect>& bounds) override {}

 private:
  void OnFactoryConnectionLost();

  mojo::Binding<ime::mojom::ImeEngineFactory> factory_binding_;
  mojo::Binding<ime::mojom::ImeEngine> engine_binding_;

  ime::mojom::ImeEngineClientPtr engine_client_;
  ime::mojom::ImeEngineFactoryRegistryPtr registry_;

  DISALLOW_COPY_AND_ASSIGN(ImeFallbackEngine);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IME_FALLBACK_ENGINE_H_
