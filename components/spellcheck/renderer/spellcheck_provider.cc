// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_provider.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_messages.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_language.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebTextCheckingCompletion.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"
#include "third_party/WebKit/public/web/WebTextDecorationType.h"

using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebString;
using blink::WebTextCheckingCompletion;
using blink::WebTextCheckingResult;
using blink::WebTextDecorationType;
using blink::WebVector;

static_assert(int(blink::kWebTextDecorationTypeSpelling) ==
                  int(SpellCheckResult::SPELLING),
              "mismatching enums");
static_assert(int(blink::kWebTextDecorationTypeGrammar) ==
                  int(SpellCheckResult::GRAMMAR),
              "mismatching enums");

SpellCheckProvider::SpellCheckProvider(
    content::RenderFrame* render_frame,
    SpellCheck* spellcheck,
    service_manager::LocalInterfaceProvider* embedder_provider)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<SpellCheckProvider>(render_frame),
      spellcheck_(spellcheck),
      embedder_provider_(embedder_provider) {
  DCHECK(spellcheck_);
  if (render_frame)  // NULL in unit tests.
    render_frame->GetWebFrame()->SetTextCheckClient(this);
}

SpellCheckProvider::~SpellCheckProvider() {
}

spellcheck::mojom::SpellCheckHost& SpellCheckProvider::GetSpellCheckHost() {
  if (spell_check_host_)
    return *spell_check_host_;

  // nullptr in tests.
  if (embedder_provider_)
    embedder_provider_->GetInterface(&spell_check_host_);
  return *spell_check_host_;
}

void SpellCheckProvider::RequestTextChecking(
    const base::string16& text,
    WebTextCheckingCompletion* completion) {
  // Ignore invalid requests.
  if (text.empty() || !HasWordCharacters(text, 0)) {
    completion->DidCancelCheckingText();
    return;
  }

  // Try to satisfy check from cache.
  if (SatisfyRequestFromCache(text, completion))
    return;

  // Send this text to a browser. A browser checks the user profile and send
  // this text to the Spelling service only if a user enables this feature.
  last_request_.clear();
  last_results_.Assign(blink::WebVector<blink::WebTextCheckingResult>());
  last_identifier_ = text_check_completions_.Add(completion);

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // TODO(crbug.com/714480): convert the RequestTextCheck IPC to mojo.
  // Text check (unified request for grammar and spell check) is only
  // available for browser process, so we ask the system spellchecker
  // over IPC or return an empty result if the checker is not available.
  Send(new SpellCheckHostMsg_RequestTextCheck(routing_id(), last_identifier_,
                                              text));
#else
  if (!spell_check_host_ && !content::RenderThread::Get())
    return;  // NULL in tests that do not provide a spell_check_host_.
  GetSpellCheckHost().CallSpellingService(
      text, base::BindOnce(&SpellCheckProvider::OnRespondSpellingService,
                           base::Unretained(this), last_identifier_, text));
#endif  // !USE_BROWSER_SPELLCHECKER
}

bool SpellCheckProvider::OnMessageReceived(const IPC::Message& message) {
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpellCheckProvider, message)
    // TODO(crbug.com/714480): convert the RequestTextCheck IPC to mojo.
    IPC_MESSAGE_HANDLER(SpellCheckMsg_RespondTextCheck, OnRespondTextCheck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
#else
  return false;
#endif
}

void SpellCheckProvider::FocusedNodeChanged(const blink::WebNode& unused) {
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  WebElement element = frame->GetDocument().IsNull()
                           ? WebElement()
                           : frame->GetDocument().FocusedElement();
  bool enabled = !element.IsNull() && element.IsEditable();
  bool checked = enabled && IsSpellCheckingEnabled();

  // TODO(crbug.com/714480): convert the ToggleSpellCheck IPC to mojo.
  Send(new SpellCheckHostMsg_ToggleSpellCheck(routing_id(), enabled, checked));
#endif  // USE_BROWSER_SPELLCHECKER
}

bool SpellCheckProvider::IsSpellCheckingEnabled() const {
  return spellcheck_->IsSpellcheckEnabled();
}

void SpellCheckProvider::CheckSpelling(
    const WebString& text,
    int& offset,
    int& length,
    WebVector<WebString>* optional_suggestions) {
  base::string16 word = text.Utf16();
  std::vector<base::string16> suggestions;
  const int kWordStart = 0;
  spellcheck_->SpellCheckWord(word.c_str(), kWordStart, word.size(),
                              routing_id(), &offset, &length,
                              optional_suggestions ? &suggestions : nullptr);
  if (optional_suggestions) {
    WebVector<WebString> web_suggestions(suggestions.size());
    std::transform(
        suggestions.begin(), suggestions.end(), web_suggestions.begin(),
        [](const base::string16& s) { return WebString::FromUTF16(s); });
    *optional_suggestions = web_suggestions;
    UMA_HISTOGRAM_COUNTS("SpellCheck.api.check.suggestions", word.size());
  } else {
    UMA_HISTOGRAM_COUNTS("SpellCheck.api.check", word.size());
    // If optional_suggestions is not requested, the API is called
    // for marking.  So we use this for counting markable words.
    if (!spell_check_host_ && !content::RenderThread::Get())
      return;  // NULL in tests that do not provide a spell_check_host_.
    GetSpellCheckHost().NotifyChecked(word, 0 < length);
  }
}

void SpellCheckProvider::RequestCheckingOfText(
    const WebString& text,
    WebTextCheckingCompletion* completion) {
  RequestTextChecking(text.Utf16(), completion);
  UMA_HISTOGRAM_COUNTS("SpellCheck.api.async", text.length());
}

void SpellCheckProvider::CancelAllPendingRequests() {
  for (WebTextCheckCompletions::iterator iter(&text_check_completions_);
       !iter.IsAtEnd(); iter.Advance()) {
    iter.GetCurrentValue()->DidCancelCheckingText();
  }
  text_check_completions_.Clear();
}

#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellCheckProvider::OnRespondSpellingService(
    int identifier,
    const base::string16& line,
    bool success,
    const std::vector<SpellCheckResult>& results) {
  WebTextCheckingCompletion* completion =
      text_check_completions_.Lookup(identifier);
  if (!completion)
    return;
  text_check_completions_.Remove(identifier);

  // If |success| is false, we use local spellcheck as a fallback.
  if (!success) {
    spellcheck_->RequestTextChecking(line, completion);
    return;
  }

  // Double-check the returned spellchecking results with our spellchecker to
  // visualize the differences between ours and the on-line spellchecker.
  blink::WebVector<blink::WebTextCheckingResult> textcheck_results;
  spellcheck_->CreateTextCheckingResults(SpellCheck::USE_NATIVE_CHECKER,
                                         0,
                                         line,
                                         results,
                                         &textcheck_results);
  completion->DidFinishCheckingText(textcheck_results);

  // Cache the request and the converted results.
  last_request_ = line;
  last_results_.Swap(textcheck_results);
}
#endif

bool SpellCheckProvider::HasWordCharacters(
    const base::string16& text,
    int index) const {
  const base::char16* data = text.data();
  int length = text.length();
  while (index < length) {
    uint32_t code = 0;
    U16_NEXT(data, index, length, code);
    UErrorCode error = U_ZERO_ERROR;
    if (uscript_getScript(code, &error) != USCRIPT_COMMON)
      return true;
  }
  return false;
}

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellCheckProvider::OnRespondTextCheck(
    int identifier,
    const base::string16& line,
    const std::vector<SpellCheckResult>& results) {
  // TODO(groby): Unify with SpellCheckProvider::OnRespondSpellingService
  DCHECK(spellcheck_);
  WebTextCheckingCompletion* completion =
      text_check_completions_.Lookup(identifier);
  if (!completion)
    return;
  text_check_completions_.Remove(identifier);
  blink::WebVector<blink::WebTextCheckingResult> textcheck_results;
  spellcheck_->CreateTextCheckingResults(SpellCheck::DO_NOT_MODIFY,
                                         0,
                                         line,
                                         results,
                                         &textcheck_results);
  completion->DidFinishCheckingText(textcheck_results);

  // Cache the request and the converted results.
  last_request_ = line;
  last_results_.Swap(textcheck_results);
}
#endif

bool SpellCheckProvider::SatisfyRequestFromCache(
    const base::string16& text,
    WebTextCheckingCompletion* completion) {
  size_t last_length = last_request_.length();
  if (!last_length)
    return false;

  // Send back the |last_results_| if the |last_request_| is a substring of
  // |text| and |text| does not have more words to check. Provider cannot cancel
  // the spellcheck request here, because WebKit might have discarded the
  // previous spellcheck results and erased the spelling markers in response to
  // the user editing the text.
  base::string16 request(text);
  size_t text_length = request.length();
  if (text_length >= last_length &&
      !request.compare(0, last_length, last_request_)) {
    if (text_length == last_length || !HasWordCharacters(text, last_length)) {
      completion->DidFinishCheckingText(last_results_);
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
    blink::WebVector<blink::WebTextCheckingResult> results(last_results_.Data(),
                                                           result_size);
    completion->DidFinishCheckingText(results);
    return true;
  }

  return false;
}

void SpellCheckProvider::OnDestruct() {
  delete this;
}
