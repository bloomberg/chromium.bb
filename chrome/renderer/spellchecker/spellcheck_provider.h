// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_H_
#define CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_H_

#include <vector>

#include "base/id_map.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpellCheckClient.h"

class RenderView;
class SpellCheck;
struct SpellCheckResult;

namespace WebKit {
class WebString;
class WebTextCheckingCompletion;
struct WebTextCheckingResult;
}

// This class deals with invoking browser-side spellcheck mechanism
// which is done asynchronously.
class SpellCheckProvider
    : public content::RenderViewObserver,
      public content::RenderViewObserverTracker<SpellCheckProvider>,
      public WebKit::WebSpellCheckClient {
 public:
  typedef IDMap<WebKit::WebTextCheckingCompletion> WebTextCheckCompletions;

  SpellCheckProvider(content::RenderView* render_view,
                     SpellCheck* spellcheck);
  virtual ~SpellCheckProvider();

  // Requests async spell and grammar checker to the platform text
  // checker, which is available on the browser process.
  void RequestTextChecking(
      const WebKit::WebString& text,
      WebKit::WebTextCheckingCompletion* completion);

  // The number of ongoing IPC requests.
  size_t pending_text_request_size() const {
    return text_check_completions_.size();
  }

  // Replace shared spellcheck data.
  void set_spellcheck(SpellCheck* spellcheck) { spellcheck_ = spellcheck; }

  // Enables document-wide spellchecking.
  void EnableSpellcheck(bool enabled);

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void FocusedNodeChanged(const WebKit::WebNode& node) OVERRIDE;

 private:
  friend class TestingSpellCheckProvider;

  // Tries to satisfy a spell check request from the cache in |last_request_|.
  // Returns true (and cancels/finishes the completion) if it can, false
  // if the provider should forward the query on.
  bool SatisfyRequestFromCache(const WebKit::WebString& text,
                               WebKit::WebTextCheckingCompletion* completion);

  // WebKit::WebSpellCheckClient implementation.
  virtual void spellCheck(
      const WebKit::WebString& text,
      int& offset,
      int& length,
      WebKit::WebVector<WebKit::WebString>* optional_suggestions) OVERRIDE;
  virtual void checkTextOfParagraph(
      const WebKit::WebString& text,
      WebKit::WebTextCheckingTypeMask mask,
      WebKit::WebVector<WebKit::WebTextCheckingResult>* results) OVERRIDE;

  virtual void requestCheckingOfText(
      const WebKit::WebString& text,
      const WebKit::WebVector<uint32>& markers,
      const WebKit::WebVector<unsigned>& marker_offsets,
      WebKit::WebTextCheckingCompletion* completion) OVERRIDE;

  virtual WebKit::WebString autoCorrectWord(
      const WebKit::WebString& misspelled_word) OVERRIDE;
  virtual void showSpellingUI(bool show) OVERRIDE;
  virtual bool isShowingSpellingUI() OVERRIDE;
  virtual void updateSpellingUIWithMisspelledWord(
      const WebKit::WebString& word) OVERRIDE;

#if !defined(OS_MACOSX)
  void OnRespondSpellingService(
      int identifier,
      bool succeeded,
      const string16& text,
      const std::vector<SpellCheckResult>& results);
#endif

  // Returns whether |text| has word characters, i.e. whether a spellchecker
  // needs to check this text.
  bool HasWordCharacters(const WebKit::WebString& text, int index) const;

#if defined(OS_MACOSX)
  void OnAdvanceToNextMisspelling();
  void OnRespondTextCheck(
      int identifier,
      const std::vector<SpellCheckResult>& results);
  void OnToggleSpellPanel(bool is_currently_visible);
#endif

  // Holds ongoing spellchecking operations, assigns IDs for the IPC routing.
  WebTextCheckCompletions text_check_completions_;

  // The last text sent to the browser process to spellcheck it and its
  // spellchecking results.
  string16 last_request_;
  WebKit::WebVector<WebKit::WebTextCheckingResult> last_results_;

  // True if the browser is showing the spelling panel for us.
  bool spelling_panel_visible_;

  // Weak pointer to shared (per RenderView) spellcheck data.
  SpellCheck* spellcheck_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckProvider);
};

#endif  // CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_H_
