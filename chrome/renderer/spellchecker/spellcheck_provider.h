// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_H_
#define CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_H_
#pragma once

#include <vector>

#include "base/id_map.h"
#include "chrome/renderer/render_view_observer.h"

class RenderView;
class SpellCheck;

namespace WebKit {
class WebString;
class WebTextCheckingResult;
class WebTextCheckingCompletion;
}

// This class deals with invoking browser-side spellcheck mechanism
// which is done asynchronously.
class SpellCheckProvider : public RenderViewObserver {
 public:
  typedef IDMap<WebKit::WebTextCheckingCompletion> WebTextCheckCompletions;

  SpellCheckProvider(RenderView* render_view, SpellCheck* spellcheck);
  virtual ~SpellCheckProvider();

  // Reqeusts async spell and grammar checker to the platform text
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

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  // A message handler that receives async results for RequestTextChecking().
  void OnSpellCheckerRespondTextCheck(
      int identifier,
      int tag,
      const std::vector<WebKit::WebTextCheckingResult>& results);

  // Holds ongoing spellchecking operations, assigns IDs for the IPC routing.
  WebTextCheckCompletions text_check_completions_;
  // Spellcheck implementation for use. Weak reference.
  SpellCheck* spellcheck_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckProvider);
};

#endif  // CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_PROVIDER_H_
