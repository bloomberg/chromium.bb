// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/id_map.h"
#include "base/macros.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "third_party/WebKit/public/web/WebTextCheckClient.h"

class SpellCheck;
struct SpellCheckResult;

namespace blink {
class WebTextCheckingCompletion;
struct WebTextCheckingResult;
}

// TODO(xiaochengh): Make SpellCheckProvider a RenderFrameObserver.
// This class deals with invoking browser-side spellcheck mechanism
// which is done asynchronously.
class SpellCheckProvider
    : public content::RenderViewObserver,
      public content::RenderViewObserverTracker<SpellCheckProvider>,
      public blink::WebTextCheckClient {
 public:
  using WebTextCheckCompletions = IDMap<blink::WebTextCheckingCompletion*>;

  SpellCheckProvider(content::RenderView* render_view,
                     SpellCheck* spellcheck);
  ~SpellCheckProvider() override;

  // Requests async spell and grammar checker to the platform text
  // checker, which is available on the browser process. The function does not
  // have special handling for partial words, as Blink guarantees that no
  // request is made when typing in the middle of a word.
  void RequestTextChecking(const base::string16& text,
                           blink::WebTextCheckingCompletion* completion);

  // The number of ongoing IPC requests.
  size_t pending_text_request_size() const {
    return text_check_completions_.size();
  }

  // Replace shared spellcheck data.
  void set_spellcheck(SpellCheck* spellcheck) { spellcheck_ = spellcheck; }

  // Enables document-wide spellchecking.
  void EnableSpellcheck(bool enabled);

  // RenderViewObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void FocusedNodeChanged(const blink::WebNode& node) override;

 private:
  friend class TestingSpellCheckProvider;

  // Tries to satisfy a spell check request from the cache in |last_request_|.
  // Returns true (and cancels/finishes the completion) if it can, false
  // if the provider should forward the query on.
  bool SatisfyRequestFromCache(const base::string16& text,
                               blink::WebTextCheckingCompletion* completion);

  // RenderViewObserver implementation.
  void OnDestruct() override;

  // blink::WebTextCheckClient implementation.
  void CheckSpelling(
      const blink::WebString& text,
      int& offset,
      int& length,
      blink::WebVector<blink::WebString>* optional_suggestions) override;
  void RequestCheckingOfText(
      const blink::WebString& text,
      blink::WebTextCheckingCompletion* completion) override;
  void CancelAllPendingRequests() override;

#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void OnRespondSpellingService(
      int identifier,
      bool succeeded,
      const base::string16& text,
      const std::vector<SpellCheckResult>& results);
#endif

  // Returns whether |text| has word characters, i.e. whether a spellchecker
  // needs to check this text.
  bool HasWordCharacters(const base::string16& text, int index) const;

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void OnRespondTextCheck(
      int identifier,
      const base::string16& line,
      const std::vector<SpellCheckResult>& results);
#endif

  // Holds ongoing spellchecking operations, assigns IDs for the IPC routing.
  WebTextCheckCompletions text_check_completions_;

  // The last text sent to the browser process to spellcheck it and its
  // spellchecking results.
  base::string16 last_request_;
  blink::WebVector<blink::WebTextCheckingResult> last_results_;

  // Weak pointer to shared (per RenderView) spellcheck data.
  SpellCheck* spellcheck_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckProvider);
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_H_
