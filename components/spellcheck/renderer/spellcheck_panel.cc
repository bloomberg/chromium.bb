// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_panel.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace {
// Returns the browser's SpellCheckPanelHost interface.
spellcheck::mojom::SpellCheckPanelHostPtr GetSpellCheckPanelHost() {
  spellcheck::mojom::SpellCheckPanelHostPtr spell_check_panel_host;
  DCHECK(content::RenderThread::Get());
  content::RenderThread::Get()->GetConnector()->BindInterface(
      content::mojom::kBrowserServiceName, &spell_check_panel_host);
  return spell_check_panel_host;
}
}

SpellCheckPanel::SpellCheckPanel(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      spelling_panel_visible_(false) {
  DCHECK(render_frame);
  render_frame->GetInterfaceRegistry()->AddInterface(base::Bind(
      &SpellCheckPanel::SpellCheckPanelRequest, base::Unretained(this)));
  render_frame->GetWebFrame()->SetSpellCheckPanelHostClient(this);
}

SpellCheckPanel::~SpellCheckPanel() = default;

void SpellCheckPanel::OnDestruct() {
  delete this;
}

bool SpellCheckPanel::IsShowingSpellingUI() {
  return spelling_panel_visible_;
}

void SpellCheckPanel::ShowSpellingUI(bool show) {
  UMA_HISTOGRAM_BOOLEAN("SpellCheck.api.showUI", show);
  GetSpellCheckPanelHost()->ShowSpellingPanel(show);
}

void SpellCheckPanel::UpdateSpellingUIWithMisspelledWord(
    const blink::WebString& word) {
  GetSpellCheckPanelHost()->UpdateSpellingPanelWithMisspelledWord(word.Utf16());
}

void SpellCheckPanel::SpellCheckPanelRequest(
    const service_manager::BindSourceInfo& source_info,
    spellcheck::mojom::SpellCheckPanelRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void SpellCheckPanel::AdvanceToNextMisspelling() {
  auto* render_frame = content::RenderFrameObserver::render_frame();
  DCHECK(render_frame->GetWebFrame());

  render_frame->GetWebFrame()->ExecuteCommand(
      blink::WebString::FromUTF8("AdvanceToNextMisspelling"));
}

void SpellCheckPanel::ToggleSpellPanel(bool visible) {
  auto* render_frame = content::RenderFrameObserver::render_frame();
  DCHECK(render_frame->GetWebFrame());

  // Tell our frame whether the spelling panel is visible or not so
  // that it won't need to make ipc calls later.
  spelling_panel_visible_ = visible;

  render_frame->GetWebFrame()->ExecuteCommand(
      blink::WebString::FromUTF8("ToggleSpellPanel"));
}
