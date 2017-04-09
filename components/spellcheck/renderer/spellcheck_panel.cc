// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_panel.h"

#include "base/metrics/histogram_macros.h"
#include "components/spellcheck/common/spellcheck_messages.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebString;

SpellCheckPanel::SpellCheckPanel(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<SpellCheckPanel>(render_view),
      spelling_panel_visible_(false) {
  render_view->GetWebView()->SetSpellCheckClient(this);
}

SpellCheckPanel::~SpellCheckPanel() = default;

void SpellCheckPanel::ShowSpellingUI(bool show) {
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  UMA_HISTOGRAM_BOOLEAN("SpellCheck.api.showUI", show);
  Send(new SpellCheckHostMsg_ShowSpellingPanel(routing_id(), show));
#endif
}

bool SpellCheckPanel::IsShowingSpellingUI() {
  return spelling_panel_visible_;
}

void SpellCheckPanel::UpdateSpellingUIWithMisspelledWord(
    const WebString& word) {
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  Send(new SpellCheckHostMsg_UpdateSpellingPanelWithMisspelledWord(
      routing_id(), word.Utf16()));
#endif
}

bool SpellCheckPanel::OnMessageReceived(const IPC::Message& message) {
#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  return false;
#endif
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpellCheckPanel, message)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_AdvanceToNextMisspelling,
                        OnAdvanceToNextMisspelling)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_ToggleSpellPanel, OnToggleSpellPanel)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
#endif
}

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellCheckPanel::OnAdvanceToNextMisspelling() {
  if (!render_view()->GetWebView())
    return;
  render_view()->GetWebView()->FocusedFrame()->ExecuteCommand(
      WebString::FromUTF8("AdvanceToNextMisspelling"));
}

void SpellCheckPanel::OnToggleSpellPanel(bool is_currently_visible) {
  if (!render_view()->GetWebView())
    return;
  // We need to tell the webView whether the spelling panel is visible or not so
  // that it won't need to make ipc calls later.
  spelling_panel_visible_ = is_currently_visible;
  render_view()->GetWebView()->FocusedFrame()->ExecuteCommand(
      WebString::FromUTF8("ToggleSpellPanel"));
}
#endif

void SpellCheckPanel::OnDestruct() {
  delete this;
}
