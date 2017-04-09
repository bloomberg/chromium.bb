// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PANEL_H
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PANEL_H

#include "base/macros.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "third_party/WebKit/public/web/WebSpellCheckClient.h"

// TODO(xiaochengh): Only Mac has spelling panel. Remove the #if checks in this
// class, which seem unnecessary, and make sure that this class is compiled and
// used only on Mac.
class SpellCheckPanel
    : public content::RenderViewObserver,
      public content::RenderViewObserverTracker<SpellCheckPanel>,
      public blink::WebSpellCheckClient {
 public:
  explicit SpellCheckPanel(content::RenderView* render_view);
  ~SpellCheckPanel() override;

  // RenderViewObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  // RenderViewObserver implementation.
  void OnDestruct() override;

  // blink::WebSpellCheckClient implementation.
  void ShowSpellingUI(bool show) override;
  bool IsShowingSpellingUI() override;
  void UpdateSpellingUIWithMisspelledWord(
      const blink::WebString& word) override;

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void OnAdvanceToNextMisspelling();
  void OnToggleSpellPanel(bool is_currently_visible);
#endif

  // True if the browser is showing the spelling panel for us.
  bool spelling_panel_visible_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckPanel);
};

#endif
