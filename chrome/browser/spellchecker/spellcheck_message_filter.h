// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_MESSAGE_FILTER_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_MESSAGE_FILTER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "content/public/browser/browser_message_filter.h"

struct SpellCheckResult;

// A message filter implementation that receives spell checker requests from
// SpellCheckProvider.
class SpellCheckMessageFilter : public content::BrowserMessageFilter {
 public:
  explicit SpellCheckMessageFilter(int render_process_id);

  // content::BrowserMessageFilter implementation.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  virtual ~SpellCheckMessageFilter();

  void OnSpellCheckerRequestDictionary();
  void OnNotifyChecked(const string16& word, bool misspelled);
#if !defined(OS_MACOSX)
  void OnCallSpellingService(int route_id,
                             int identifier,
                             int document_tag,
                             const string16& text);

  // A callback function called when the Spelling service finishes checking
  // text. We send the given results to a renderer.
  void OnTextCheckComplete(
      int tag,
      bool success,
      const string16& text,
      const std::vector<SpellCheckResult>& results);

  // Checks the user profile and sends a JSON-RPC request to the Spelling
  // service if a user enables the "Ask Google for suggestions" option. When we
  // receive a response (including an error) from the service, it calls
  // OnTextCheckComplete. When this function is called before we receive a
  // response for the previous request, this function cancels the previous
  // request and sends a new one.
  bool CallSpellingService(int route_id,
                           int identifier,
                           int document_tag,
                           const string16& text);
#endif

  int render_process_id_;

  // A JSON-RPC client that calls the Spelling service in the background.
  scoped_ptr<SpellingServiceClient> client_;

#if !defined(OS_MACOSX)
  int route_id_;
  int identifier_;
#endif
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_MESSAGE_FILTER_H_
