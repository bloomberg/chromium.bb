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
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_GetDocumentTag, OnGetDocumentTag)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_DocumentWithTagClosed,
                        OnDocumentWithTagClosed)
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
                                                 int tag,
                                                 bool* correct) {
  *correct = spellcheck_mac::CheckSpelling(word, tag);
}

void SpellCheckMessageFilterMac::OnFillSuggestionList(
    const string16& word,
    std::vector<string16>* suggestions) {
  spellcheck_mac::FillSuggestionList(word, suggestions);
}

void SpellCheckMessageFilterMac::OnGetDocumentTag(int* tag) {
  *tag = spellcheck_mac::GetDocumentTag();
}

void SpellCheckMessageFilterMac::OnDocumentWithTagClosed(int tag) {
  spellcheck_mac::CloseDocumentWithTag(tag);
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
    int document_tag,
    const string16& text) {
  spellcheck_mac::RequestTextCheck(
      route_id, identifier, document_tag, text, this);
}
