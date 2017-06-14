// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PANEL_H
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PANEL_H

#include "base/macros.h"
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/WebKit/public/platform/WebSpellCheckPanelHostClient.h"

#if !BUILDFLAG(HAS_SPELLCHECK_PANEL)
#error "Spellcheck panel should be enabled."
#endif

namespace service_manager {
struct BindSourceInfo;
}

class SpellCheckPanel : public content::RenderFrameObserver,
                        public blink::WebSpellCheckPanelHostClient,
                        public spellcheck::mojom::SpellCheckPanel {
 public:
  explicit SpellCheckPanel(content::RenderFrame* render_frame);
  ~SpellCheckPanel() override;

 private:
  // content::RenderFrameObserver:
  void OnDestruct() override;

  // blink::WebSpellCheckPanelHostClient:
  bool IsShowingSpellingUI() override;
  void ShowSpellingUI(bool show) override;
  void UpdateSpellingUIWithMisspelledWord(
      const blink::WebString& word) override;

  // Binds browser requests for the frame SpellCheckPanel interface.
  void SpellCheckPanelRequest(
      const service_manager::BindSourceInfo& source_info,
      spellcheck::mojom::SpellCheckPanelRequest request);

  // spellcheck::mojom::SpellCheckPanel:
  void ToggleSpellPanel(bool visible) override;
  void AdvanceToNextMisspelling() override;

  // SpellCheckPanel bindings.
  mojo::BindingSet<spellcheck::mojom::SpellCheckPanel> bindings_;

  // True if the browser is showing the spelling panel.
  bool spelling_panel_visible_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckPanel);
};

#endif
