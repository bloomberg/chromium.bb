// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/spellcheck_provider.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingResult.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingType.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebTextCheckingCompletion;
using WebKit::WebTextCheckingResult;
using WebKit::WebTextCheckingType;
using WebKit::WebVector;

COMPILE_ASSERT(int(WebKit::WebTextCheckingTypeSpelling) ==
               int(SpellCheckResult::SPELLING), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextCheckingTypeGrammar) ==
               int(SpellCheckResult::GRAMMAR), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextCheckingTypeLink) ==
               int(SpellCheckResult::LINK), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextCheckingTypeQuote) ==
               int(SpellCheckResult::QUOTE), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextCheckingTypeDash) ==
               int(SpellCheckResult::DASH), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextCheckingTypeReplacement) ==
               int(SpellCheckResult::REPLACEMENT), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextCheckingTypeCorrection) ==
               int(SpellCheckResult::CORRECTION), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextCheckingTypeShowCorrectionPanel) ==
               int(SpellCheckResult::SHOWCORRECTIONPANEL), mismatching_enums);

SpellCheckProvider::SpellCheckProvider(
    content::RenderView* render_view,
    SpellCheck* spellcheck)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<SpellCheckProvider>(render_view),
      spelling_panel_visible_(false),
      spellcheck_(spellcheck) {
  DCHECK(spellcheck_);
  if (render_view) {  // NULL in unit tests.
    render_view->GetWebView()->setSpellCheckClient(this);
    EnableSpellcheck(spellcheck_->is_spellcheck_enabled());
  }
}

SpellCheckProvider::~SpellCheckProvider() {
#if defined(OS_MACOSX)
  Send(new SpellCheckHostMsg_DocumentClosed(routing_id(), routing_id()));
#endif
}

void SpellCheckProvider::RequestTextChecking(
    const WebString& text,
    WebTextCheckingCompletion* completion) {
#if defined(OS_MACOSX)
  // Text check (unified request for grammar and spell check) is only
  // available for browser process, so we ask the system spellchecker
  // over IPC or return an empty result if the checker is not
  // available.
  Send(new SpellCheckHostMsg_RequestTextCheck(
      routing_id(),
      text_check_completions_.Add(completion),
      text));
#else
  // Ignore invalid requests.
  // TODO(groby): Should that be applied for OSX, too?
  if (text.isEmpty() || !HasWordCharacters(text, 0)) {
    completion->didCancelCheckingText();
    return;
  }

  // Try to satisfy check from cache.
  // TODO(groby): Should that be applied to OSX results, too?
  if (SatisfyRequestFromCache(text, completion))
    return;

  // Send this text to a browser. A browser checks the user profile and send
  // this text to the Spelling service only if a user enables this feature.
  last_request_.clear();
  last_results_.assign(WebKit::WebVector<WebKit::WebTextCheckingResult>());

  Send(new SpellCheckHostMsg_CallSpellingService(
      routing_id(),
      text_check_completions_.Add(completion),
      0,
      string16(text)));
#endif  // !OS_MACOSX
}

bool SpellCheckProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpellCheckProvider, message)
#if !defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_RespondSpellingService,
                        OnRespondSpellingService)
#endif
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_AdvanceToNextMisspelling,
                        OnAdvanceToNextMisspelling)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_RespondTextCheck, OnRespondTextCheck)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_ToggleSpellPanel, OnToggleSpellPanel)
#endif
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
  string16 word(text);
  std::vector<string16> suggestions;
  spellcheck_->SpellCheckWord(
      word.c_str(), word.size(), routing_id(),
      &offset, &length, optional_suggestions ? & suggestions : NULL);
  if (optional_suggestions) {
    *optional_suggestions = suggestions;
    UMA_HISTOGRAM_COUNTS("SpellCheck.api.check.suggestions", word.size());
  } else {
    UMA_HISTOGRAM_COUNTS("SpellCheck.api.check", word.size());
    // If optional_suggestions is not requested, the API is called
    // for marking.  So we use this for counting markable words.
    Send(new SpellCheckHostMsg_NotifyChecked(routing_id(), word, 0 < length));
  }
}

void SpellCheckProvider::checkTextOfParagraph(
    const WebKit::WebString& text,
    WebKit::WebTextCheckingTypeMask mask,
    WebKit::WebVector<WebKit::WebTextCheckingResult>* results) {
#if !defined(OS_MACOSX)
  // Since Mac has its own spell checker, this method will not be used on Mac.

  if (!results)
    return;

  if (!(mask & WebKit::WebTextCheckingTypeSpelling))
    return;

  spellcheck_->SpellCheckParagraph(string16(text), results);
  UMA_HISTOGRAM_COUNTS("SpellCheck.api.paragraph", text.length());
#endif
}

void SpellCheckProvider::requestCheckingOfText(
    const WebString& text,
    WebTextCheckingCompletion* completion) {
  RequestTextChecking(text, completion);
  UMA_HISTOGRAM_COUNTS("SpellCheck.api.async", text.length());
}

WebString SpellCheckProvider::autoCorrectWord(const WebString& word) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableSpellingAutoCorrect)) {
    UMA_HISTOGRAM_COUNTS("SpellCheck.api.autocorrect", word.length());
    return spellcheck_->GetAutoCorrectionWord(word, routing_id());
  }
  return string16();
}

void SpellCheckProvider::showSpellingUI(bool show) {
#if defined(OS_MACOSX)
  UMA_HISTOGRAM_BOOLEAN("SpellCheck.api.showUI", show);
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

#if !defined(OS_MACOSX)
void SpellCheckProvider::OnRespondSpellingService(
    int identifier,
    int offset,
    bool succeeded,
    const string16& line,
    const std::vector<SpellCheckResult>& results) {
  WebTextCheckingCompletion* completion =
      text_check_completions_.Lookup(identifier);
  if (!completion)
    return;
  text_check_completions_.Remove(identifier);

  // If |succeeded| is false, we use local spellcheck as a fallback.
  if (!succeeded) {
    spellcheck_->RequestTextChecking(line, offset, completion);
    return;
  }

  // Double-check the returned spellchecking results with our spellchecker to
  // visualize the differences between ours and the on-line spellchecker.
  WebKit::WebVector<WebKit::WebTextCheckingResult> textcheck_results;
  spellcheck_->CreateTextCheckingResults(SpellCheck::USE_NATIVE_CHECKER,
                                         offset,
                                         line,
                                         results,
                                         &textcheck_results);
  completion->didFinishCheckingText(textcheck_results);

  // Cache the request and the converted results.
  last_request_ = line;
  last_results_.swap(textcheck_results);
}

bool SpellCheckProvider::HasWordCharacters(
    const WebKit::WebString& text,
    int index) const {
  const char16* data = text.data();
  int length = text.length();
  while (index < length) {
    uint32 code = 0;
    U16_NEXT(data, index, length, code);
    UErrorCode error = U_ZERO_ERROR;
    if (uscript_getScript(code, &error) != USCRIPT_COMMON)
      return true;
  }
  return false;
}
#endif

#if defined(OS_MACOSX)
void SpellCheckProvider::OnAdvanceToNextMisspelling() {
  if (!render_view()->GetWebView())
    return;
  render_view()->GetWebView()->focusedFrame()->executeCommand(
      WebString::fromUTF8("AdvanceToNextMisspelling"));
}

void SpellCheckProvider::OnRespondTextCheck(
    int identifier,
    const std::vector<SpellCheckResult>& results) {
  DCHECK(spellcheck_);
  WebTextCheckingCompletion* completion =
      text_check_completions_.Lookup(identifier);
  if (!completion)
    return;
  text_check_completions_.Remove(identifier);
  WebKit::WebVector<WebKit::WebTextCheckingResult> textcheck_results;
  spellcheck_->CreateTextCheckingResults(SpellCheck::DO_NOT_MODIFY,
                                         0,
                                         string16(),
                                         results,
                                         &textcheck_results);
  completion->didFinishCheckingText(textcheck_results);
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
#endif

void SpellCheckProvider::EnableSpellcheck(bool enable) {
  if (!render_view()->GetWebView())
    return;

  WebFrame* frame = render_view()->GetWebView()->focusedFrame();
  frame->enableContinuousSpellChecking(enable);
}

#if !defined(OS_MACOSX)
bool SpellCheckProvider::SatisfyRequestFromCache(
    const WebString& text,
    WebTextCheckingCompletion* completion) {
  // Cancel this spellcheck request if the cached text is a substring of the
  // given text and the given text is the middle of a possible word.
  string16 request(text);
  size_t text_length = request.length();
  size_t last_length = last_request_.length();
  if (text_length >= last_length &&
      !request.compare(0, last_length, last_request_)) {
    if (text_length == last_length || !HasWordCharacters(text, last_length)) {
      completion->didCancelCheckingText();
      return true;
    }
    int code = 0;
    int length = static_cast<int>(text_length);
    U16_PREV(text.data(), 0, length, code);
    UErrorCode error = U_ZERO_ERROR;
    if (uscript_getScript(code, &error) != USCRIPT_COMMON) {
      completion->didCancelCheckingText();
      return true;
    }
  }
  // Create a subset of the cached results and return it if the given text is a
  // substring of the cached text.
  if (text_length < last_length &&
      !last_request_.compare(0, text_length, request)) {
    size_t result_size = 0;
    for (size_t i = 0; i < last_results_.size(); ++i) {
      size_t start = last_results_[i].location;
      size_t end = start + last_results_[i].length;
      if (start <= text_length && end <= text_length)
        ++result_size;
    }
    if (result_size > 0) {
      WebKit::WebVector<WebKit::WebTextCheckingResult> results(result_size);
      for (size_t i = 0; i < result_size; ++i) {
        results[i].type = last_results_[i].type;
        results[i].location = last_results_[i].location;
        results[i].length = last_results_[i].length;
        results[i].replacement = last_results_[i].replacement;
      }
      completion->didFinishCheckingText(results);
      return true;
    }
  }

  return false;
}
#endif
