// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/spellcheck_provider.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingResult.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"

using WebKit::WebString;
using WebKit::WebTextCheckingCompletion;
using WebKit::WebTextCheckingResult;

SpellCheckProvider::SpellCheckProvider(RenderView* render_view,
                                       SpellCheck* spellcheck)
    : RenderViewObserver(render_view),
      spellcheck_(spellcheck) {
}

SpellCheckProvider::~SpellCheckProvider() {
}

void SpellCheckProvider::RequestTextChecking(
    const WebString& text,
    int document_tag,
    WebTextCheckingCompletion* completion) {
  // Text check (unified request for grammar and spell check) is only
  // available for browser process, so we ask the system sellchecker
  // over IPC or return an empty result if the checker is not
  // available.
  if (!is_using_platform_spelling_engine()) {
    completion->didFinishCheckingText
        (std::vector<WebTextCheckingResult>());
    return;
  }

  Send(new ViewHostMsg_SpellChecker_PlatformRequestTextCheck(
      routing_id(),
      text_check_completions_.Add(completion),
      document_tag,
      text));
}

void SpellCheckProvider::OnSpellCheckerRespondTextCheck(
    int identifier,
    int tag,
    const std::vector<WebTextCheckingResult>& results) {
  WebKit::WebTextCheckingCompletion* completion =
      text_check_completions_.Lookup(identifier);
  if (!completion)
    return;
  text_check_completions_.Remove(identifier);
  completion->didFinishCheckingText(results);
}

bool SpellCheckProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpellCheckProvider, message)
    IPC_MESSAGE_HANDLER(ViewMsg_SpellChecker_RespondTextCheck,
                        OnSpellCheckerRespondTextCheck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool SpellCheckProvider::is_using_platform_spelling_engine() const {
  return spellcheck_ && spellcheck_->is_using_platform_spelling_engine();
}
