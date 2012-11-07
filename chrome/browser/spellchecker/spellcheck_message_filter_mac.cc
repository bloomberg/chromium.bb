// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_message_filter_mac.h"

#include "chrome/browser/spellchecker/spellcheck_platform_mac.h"
#include "chrome/common/spellcheck_messages.h"

using content::BrowserThread;

SpellCheckMessageFilterMac::SpellCheckMessageFilterMac() {}

bool SpellCheckMessageFilterMac::OnMessageReceived(const IPC::Message& message,
                                                   bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SpellCheckMessageFilterMac, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_CheckSpelling,
                        OnCheckSpelling)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_FillSuggestionList,
                        OnFillSuggestionList)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_DocumentClosed,
                        OnDocumentClosed)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_ShowSpellingPanel,
                        OnShowSpellingPanel)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_UpdateSpellingPanelWithMisspelledWord,
                        OnUpdateSpellingPanelWithMisspelledWord)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_RequestTextCheck,
                        OnRequestTextCheck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

SpellCheckMessageFilterMac::~SpellCheckMessageFilterMac() {}

void SpellCheckMessageFilterMac::OnCheckSpelling(const string16& word,
                                                 int route_id,
                                                 bool* correct) {
  *correct = spellcheck_mac::CheckSpelling(word, ToDocumentTag(route_id));
}

void SpellCheckMessageFilterMac::OnFillSuggestionList(
    const string16& word,
    std::vector<string16>* suggestions) {
  spellcheck_mac::FillSuggestionList(word, suggestions);
}


void SpellCheckMessageFilterMac::OnDocumentClosed(int route_id) {
  spellcheck_mac::CloseDocumentWithTag(ToDocumentTag(route_id));
  RetireDocumentTag(route_id);
}

void SpellCheckMessageFilterMac::OnShowSpellingPanel(bool show) {
  spellcheck_mac::ShowSpellingPanel(show);
}

void SpellCheckMessageFilterMac::OnUpdateSpellingPanelWithMisspelledWord(
    const string16& word) {
  spellcheck_mac::UpdateSpellingPanelWithMisspelledWord(word);
}

void SpellCheckMessageFilterMac::OnRequestTextCheck(
    int route_id,
    int identifier,
    const string16& text) {
  spellcheck_mac::RequestTextCheck(
      route_id, identifier, ToDocumentTag(route_id), text, this);
}

int SpellCheckMessageFilterMac::ToDocumentTag(int route_id) {
  if (!tag_map_.count(route_id))
    tag_map_[route_id] = spellcheck_mac::GetDocumentTag();
  return tag_map_[route_id];
}

void SpellCheckMessageFilterMac::RetireDocumentTag(int route_id) {
  tag_map_.erase(route_id);
}

