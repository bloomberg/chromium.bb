// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/spellcheck_provider.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingResult.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebTextCheckingCompletion;
using WebKit::WebTextCheckingResult;
using WebKit::WebVector;

SpellCheckProvider::SpellCheckProvider(
    content::RenderView* render_view,
    chrome::ChromeContentRendererClient* renderer_client)
    : content::RenderViewObserver(render_view),
#if defined(OS_MACOSX)
      has_document_tag_(false),
#endif
      document_tag_(0),
      spelling_panel_visible_(false),
      chrome_content_renderer_client_(renderer_client) {
  if (render_view)  // NULL in unit tests.
    render_view->GetWebView()->setSpellCheckClient(this);
}

SpellCheckProvider::~SpellCheckProvider() {
#if defined(OS_MACOSX)
  // Tell the spellchecker that the document is closed.
  if (has_document_tag_) {
    Send(new SpellCheckHostMsg_DocumentWithTagClosed(
        routing_id(), document_tag_));
  }
#endif
}

void SpellCheckProvider::RequestTextChecking(
    const WebString& text,
    int document_tag,
    WebTextCheckingCompletion* completion) {
#if defined(OS_MACOSX)
  // Text check (unified request for grammar and spell check) is only
  // available for browser process, so we ask the system spellchecker
  // over IPC or return an empty result if the checker is not
  // available.
  Send(new SpellCheckHostMsg_RequestTextCheck(
      routing_id(),
      text_check_completions_.Add(completion),
      document_tag,
      text));
#else
    completion->didFinishCheckingText(
        std::vector<WebTextCheckingResult>());
#endif  // !OS_MACOSX
}

bool SpellCheckProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpellCheckProvider, message)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_AdvanceToNextMisspelling,
                        OnAdvanceToNextMisspelling)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_RespondTextCheck, OnRespondTextCheck)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_ToggleSpellPanel, OnToggleSpellPanel)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_ToggleSpellCheck, OnToggleSpellCheck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SpellCheckProvider::FocusedNodeChanged(const WebKit::WebNode& unused) {
#if defined(OS_MACOSX)
  bool enabled = false;
  WebKit::WebNode node = render_view()->GetFocusedNode();
  if (!node.isNull())
    enabled = render_view()->IsEditableNode(node);

  bool checked = false;
  if (enabled && render_view()->GetWebView()) {
    WebFrame* frame = render_view()->GetWebView()->focusedFrame();
    if (frame->isContinuousSpellCheckingEnabled())
      checked = true;
  }

  Send(new SpellCheckHostMsg_ToggleSpellCheck(routing_id(), enabled, checked));
#endif  // OS_MACOSX
}

void SpellCheckProvider::spellCheck(
    const WebString& text,
    int& offset,
    int& length,
    WebVector<WebString>* optional_suggestions) {
  EnsureDocumentTag();

  string16 word(text);
  // Will be NULL during unit tests.
  if (chrome_content_renderer_client_) {
    std::vector<string16> suggestions;
    chrome_content_renderer_client_->spellcheck()->SpellCheckWord(
        word.c_str(), word.size(), document_tag_,
        &offset, &length, optional_suggestions ? & suggestions : NULL);
    if (optional_suggestions)
      *optional_suggestions = suggestions;
    if (!optional_suggestions) {
      // If optional_suggestions is not requested, the API is called
      // for marking.  So we use this for counting markable words.
      Send(new SpellCheckHostMsg_NotifyChecked(routing_id(), word, 0 < length));
    }
  }
}

void SpellCheckProvider::requestCheckingOfText(
    const WebString& text,
    WebTextCheckingCompletion* completion) {
  RequestTextChecking(text, document_tag_, completion);
}

WebString SpellCheckProvider::autoCorrectWord(const WebString& word) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kExperimentalSpellcheckerFeatures)) {
    EnsureDocumentTag();
    // Will be NULL during unit tests.
    if (chrome_content_renderer_client_) {
      return chrome_content_renderer_client_->spellcheck()->
          GetAutoCorrectionWord(word, document_tag_);
    }
  }
  return string16();
}

void SpellCheckProvider::showSpellingUI(bool show) {
#if defined(OS_MACOSX)
  Send(new SpellCheckHostMsg_ShowSpellingPanel(routing_id(), show));
#endif
}

bool SpellCheckProvider::isShowingSpellingUI() {
  return spelling_panel_visible_;
}

void SpellCheckProvider::updateSpellingUIWithMisspelledWord(
    const WebString& word) {
#if defined(OS_MACOSX)
  Send(new SpellCheckHostMsg_UpdateSpellingPanelWithMisspelledWord(routing_id(),
                                                                   word));
#endif
}

void SpellCheckProvider::OnAdvanceToNextMisspelling() {
  if (!render_view()->GetWebView())
    return;
  render_view()->GetWebView()->focusedFrame()->executeCommand(
      WebString::fromUTF8("AdvanceToNextMisspelling"));
}

void SpellCheckProvider::OnRespondTextCheck(
    int identifier,
    int tag,
    const std::vector<WebTextCheckingResult>& results) {
  WebTextCheckingCompletion* completion =
      text_check_completions_.Lookup(identifier);
  if (!completion)
    return;
  text_check_completions_.Remove(identifier);
  completion->didFinishCheckingText(results);
}

void SpellCheckProvider::OnToggleSpellPanel(bool is_currently_visible) {
  if (!render_view()->GetWebView())
    return;
  // We need to tell the webView whether the spelling panel is visible or not so
  // that it won't need to make ipc calls later.
  spelling_panel_visible_ = is_currently_visible;
  render_view()->GetWebView()->focusedFrame()->executeCommand(
      WebString::fromUTF8("ToggleSpellPanel"));
}

void SpellCheckProvider::OnToggleSpellCheck() {
  if (!render_view()->GetWebView())
    return;

  WebFrame* frame = render_view()->GetWebView()->focusedFrame();
  frame->enableContinuousSpellChecking(
      !frame->isContinuousSpellCheckingEnabled());
}

void SpellCheckProvider::EnsureDocumentTag() {
  // TODO(darin): There's actually no reason for this to be here.  We should
  // have the browser side manage the document tag.
#if defined(OS_MACOSX)
  if (!has_document_tag_) {
    // Make the call to get the tag.
    Send(new SpellCheckHostMsg_GetDocumentTag(routing_id(), &document_tag_));
    has_document_tag_ = true;
  }
#endif
}
