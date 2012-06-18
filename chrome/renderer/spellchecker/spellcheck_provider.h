// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_H_
#define CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_H_
#pragma once

#include <vector>

#include "base/id_map.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpellCheckClient.h"

class RenderView;
struct SpellCheckResult;

namespace chrome {
class ChromeContentRendererClient;
}

namespace WebKit {
class WebString;
class WebTextCheckingCompletion;
struct WebTextCheckingResult;
}

// This class deals with invoking browser-side spellcheck mechanism
// which is done asynchronously.
class SpellCheckProvider : public content::RenderViewObserver,
                           public WebKit::WebSpellCheckClient {
 public:
  typedef IDMap<WebKit::WebTextCheckingCompletion> WebTextCheckCompletions;

  SpellCheckProvider(content::RenderView* render_view,
                     chrome::ChromeContentRendererClient* render_client);
  virtual ~SpellCheckProvider();

  // Requests async spell and grammar checker to the platform text
  // checker, which is available on the browser process.
  void RequestTextChecking(
      const WebKit::WebString& text,
      int document_tag,
      WebKit::WebTextCheckingCompletion* completion);

  // The number of ongoing IPC requests.
  size_t pending_text_request_size() const {
    return text_check_completions_.size();
  }

  int document_tag() const { return document_tag_; }

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void FocusedNodeChanged(const WebKit::WebNode& node) OVERRIDE;

 private:
  friend class TestingSpellCheckProvider;

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
      WebKit::WebTextCheckingCompletion* completion) OVERRIDE;
  virtual WebKit::WebString autoCorrectWord(
      const WebKit::WebString& misspelled_word) OVERRIDE;
  virtual void showSpellingUI(bool show) OVERRIDE;
  virtual bool isShowingSpellingUI() OVERRIDE;
  virtual void updateSpellingUIWithMisspelledWord(
      const WebKit::WebString& word) OVERRIDE;

  // Replaces the misspelling range that covers the cursor with the specified
  // text. This function does not do anything if there are not any misspelling
  // ranges that covers the cursor.
  void OnReplace(const string16& text);

#if !defined(OS_MACOSX)
  void OnRespondSpellingService(
      int identifier,
      int offset,
      bool succeeded,
      const string16& text,
      const std::vector<SpellCheckResult>& results);

  // Returns whether |text| has word characters, i.e. whether a spellchecker
  // needs to check this text.
  bool HasWordCharacters(const WebKit::WebString& text, int index) const;
#endif
#if defined(OS_MACOSX)
  void OnAdvanceToNextMisspelling();
  void OnRespondTextCheck(
      int identifier,
      int tag,
      const std::vector<SpellCheckResult>& results);
  void OnToggleSpellPanel(bool is_currently_visible);
#endif
  void OnToggleSpellCheck();

  // Initializes the document_tag_ member if necessary.
  void EnsureDocumentTag();

  // Holds ongoing spellchecking operations, assigns IDs for the IPC routing.
  WebTextCheckCompletions text_check_completions_;

#if !defined(OS_MACOSX)
  // The last text sent to the browser process to spellcheck it and its
  // spellchecking results.
  string16 last_request_;
  WebKit::WebVector<WebKit::WebTextCheckingResult> last_results_;
#endif

#if defined(OS_MACOSX)
  // True if the current RenderView has been assigned a document tag.
  bool has_document_tag_;
#endif

  int document_tag_;

  // True if the browser is showing the spelling panel for us.
  bool spelling_panel_visible_;

  // The ChromeContentRendererClient used to access the SpellChecker.
  // Weak reference.
  chrome::ChromeContentRendererClient* chrome_content_renderer_client_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckProvider);
};

#endif  // CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_H_
