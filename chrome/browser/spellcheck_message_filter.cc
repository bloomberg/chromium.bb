// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellcheck_message_filter.h"

#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/common/spellcheck_messages.h"

SpellCheckMessageFilter::SpellCheckMessageFilter() {
}

SpellCheckMessageFilter::~SpellCheckMessageFilter() {
}

bool SpellCheckMessageFilter::OnMessageReceived(const IPC::Message& message,
                                          bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SpellCheckMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_PlatformCheckSpelling,
                        OnPlatformCheckSpelling)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_PlatformFillSuggestionList,
                        OnPlatformFillSuggestionList)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_GetDocumentTag, OnGetDocumentTag)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_DocumentWithTagClosed,
                        OnDocumentWithTagClosed)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_ShowSpellingPanel,
                        OnShowSpellingPanel)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_UpdateSpellingPanelWithMisspelledWord,
                        OnUpdateSpellingPanelWithMisspelledWord)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_PlatformRequestTextCheck,
                        OnPlatformRequestTextCheck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SpellCheckMessageFilter::OnPlatformCheckSpelling(const string16& word,
                                                      int tag,
                                                      bool* correct) {
  *correct = SpellCheckerPlatform::CheckSpelling(word, tag);
}

void SpellCheckMessageFilter::OnPlatformFillSuggestionList(
    const string16& word,
    std::vector<string16>* suggestions) {
  SpellCheckerPlatform::FillSuggestionList(word, suggestions);
}

void SpellCheckMessageFilter::OnGetDocumentTag(int* tag) {
  *tag = SpellCheckerPlatform::GetDocumentTag();
}

void SpellCheckMessageFilter::OnDocumentWithTagClosed(int tag) {
  SpellCheckerPlatform::CloseDocumentWithTag(tag);
}

void SpellCheckMessageFilter::OnShowSpellingPanel(bool show) {
  SpellCheckerPlatform::ShowSpellingPanel(show);
}

void SpellCheckMessageFilter::OnUpdateSpellingPanelWithMisspelledWord(
    const string16& word) {
  SpellCheckerPlatform::UpdateSpellingPanelWithMisspelledWord(word);
}

void SpellCheckMessageFilter::OnPlatformRequestTextCheck(
    int route_id,
    int identifier,
    int document_tag,
    const string16& text) {
  SpellCheckerPlatform::RequestTextCheck(
      route_id, identifier, document_tag, text, this);
}
