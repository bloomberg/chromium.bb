// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBCOLORCHOOSER_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBCOLORCHOOSER_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/color_chooser.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/web/WebColorChooser.h"
#include "third_party/WebKit/public/web/WebColorChooserClient.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {

class RendererWebColorChooserImpl : public blink::WebColorChooser,
                                    public mojom::ColorChooserClient,
                                    public RenderFrameObserver {
 public:
  explicit RendererWebColorChooserImpl(RenderFrame* render_frame,
                                       blink::WebColorChooserClient*);
  ~RendererWebColorChooserImpl() override;

  // blink::WebColorChooser implementation:
  void SetSelectedColor(const blink::WebColor) override;
  void EndChooser() override;

  void Open(SkColor initial_color,
            std::vector<mojom::ColorSuggestionPtr> suggestions);

 private:
  // RenderFrameObserver implementation:
  // Don't destroy the RendererWebColorChooserImpl when the RenderFrame goes
  // away. RendererWebColorChooserImpl is owned by
  // blink::ColorChooserUIController.
  void OnDestruct() override {}

  // content::mojom::ColorChooserClient
  void DidChooseColor(SkColor color) override;

  blink::WebColorChooserClient* blink_client_;
  mojom::ColorChooserFactoryPtr color_chooser_factory_;
  mojom::ColorChooserPtr color_chooser_;
  mojo::Binding<mojom::ColorChooserClient> mojo_client_binding_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebColorChooserImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_WEBCOLORCHOOSER_IMPL_H_
