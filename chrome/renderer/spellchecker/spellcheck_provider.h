// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
class SpellCheck;

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

  SpellCheckProvider(content::RenderView* render_view, SpellCheck* spellcheck);
  virtual ~SpellCheckProvider();

  // Requests async spell and grammar checker to the platform text
  // checker, which is available on the browser process.
  void RequestTextChecking(
      const WebKit::WebString& text,
      int document_tag,
      WebKit::WebTextCheckingCompletion* completion);

  // Check the availability of the platform spellchecker.
  // Makes this virtual for overriding on the unittest.
  virtual bool is_using_platform_spelling_engine() const;

  // The number of ongoing IPC requests.
  size_t pending_text_request_size() const {
    return text_check_completions_.size();
  }

  int document_tag() const { return document_tag_; }

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void FocusedNodeChanged(const WebKit::WebNode& node) OVERRIDE;

  void SetSpellCheck(SpellCheck* spellcheck);

 private:
  // WebKit::WebSpellCheckClient implementation.
  virtual void spellCheck(
      const WebKit::WebString& text,
      int& offset,
      int& length,
      WebKit::WebVector<WebKit::WebString>* optional_suggestions);
  virtual void requestCheckingOfText(
      const WebKit::WebString& text,
      WebKit::WebTextCheckingCompletion* completion);
  virtual WebKit::WebString autoCorrectWord(
      const WebKit::WebString& misspelled_word);
  virtual void showSpellingUI(bool show);
  virtual bool isShowingSpellingUI();
  virtual void updateSpellingUIWithMisspelledWord(
      const WebKit::WebString& word);

  void OnAdvanceToNextMisspelling();
  void OnRespondTextCheck(
      int identifier,
      int tag,
      const std::vector<WebKit::WebTextCheckingResult>& results);
  void OnToggleSpellCheck();
  void OnToggleSpellPanel(bool is_currently_visible);

  // Initializes the document_tag_ member if necessary.
  void EnsureDocumentTag();

  // Holds ongoing spellchecking operations, assigns IDs for the IPC routing.
  WebTextCheckCompletions text_check_completions_;

#if defined(OS_MACOSX)
  // True if the current RenderView has been assigned a document tag.
  bool has_document_tag_;
#endif

  int document_tag_;

  // True if the browser is showing the spelling panel for us.
  bool spelling_panel_visible_;

  // Spellcheck implementation for use. Weak reference.
  SpellCheck* spellcheck_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckProvider);
};

#endif  // CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_H_
