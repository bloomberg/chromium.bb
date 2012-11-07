// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_MESSAGE_FILTER_MAC_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_MESSAGE_FILTER_MAC_H_

#include <map>

#include "content/public/browser/browser_message_filter.h"

// A message filter implementation that receives
// the Mac-specific spell checker requests from SpellCheckProvider.
class SpellCheckMessageFilterMac : public content::BrowserMessageFilter {
 public:
  explicit SpellCheckMessageFilterMac();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  friend class TestingSpellCheckMessageFilter;

  virtual ~SpellCheckMessageFilterMac();

  void OnCheckSpelling(const string16& word, int route_id, bool* correct);
  void OnFillSuggestionList(const string16& word,
                            std::vector<string16>* suggestions);
  void OnDocumentClosed(int route_id);
  void OnShowSpellingPanel(bool show);
  void OnUpdateSpellingPanelWithMisspelledWord(const string16& word);
  void OnRequestTextCheck(int route_id,
                          int identifier,
                          const string16& text);

  int ToDocumentTag(int route_id);
  void RetireDocumentTag(int route_id);
  std::map<int,int> tag_map_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckMessageFilterMac);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_MESSAGE_FILTER_MAC_H_
