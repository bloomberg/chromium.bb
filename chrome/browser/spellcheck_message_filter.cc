// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellcheck_message_filter.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellcheck_host.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/browser/renderer_host/render_process_host.h"

SpellCheckMessageFilter::SpellCheckMessageFilter(int render_process_id)
    : render_process_id_(render_process_id) {
}

SpellCheckMessageFilter::~SpellCheckMessageFilter() {
}

void SpellCheckMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (message.type() == SpellCheckHostMsg_RequestDictionary::ID ||
      message.type() == SpellCheckHostMsg_NotifyChecked::ID)
    *thread = BrowserThread::UI;
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
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_RequestDictionary,
                        OnSpellCheckerRequestDictionary)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_NotifyChecked,
                        OnNotifyChecked)
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

void SpellCheckMessageFilter::OnSpellCheckerRequestDictionary() {
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);
  if (!host)
    return;  // Teardown.
  Profile* profile = host->profile();
  // The renderer has requested that we initialize its spellchecker. This should
  // generally only be called once per session, as after the first call, all
  // future renderers will be passed the initialization information on startup
  // (or when the dictionary changes in some way).
  if (profile->GetSpellCheckHost()) {
    // The spellchecker initialization already started and finished; just send
    // it to the renderer.
    profile->GetSpellCheckHost()->InitForRenderer(host);
  } else {
    // We may have gotten multiple requests from different renderers. We don't
    // want to initialize multiple times in this case, so we set |force| to
    // false.
    profile->ReinitializeSpellCheckHost(false);
  }
}

void SpellCheckMessageFilter::OnNotifyChecked(bool misspelled) {
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);
  if (!host)
    return;  // Teardown.
  // Delegates to SpellCheckHost which tracks the stats of our spellchecker.
  Profile* profile = host->profile();
  if (profile->GetSpellCheckHost())
    profile->GetSpellCheckHost()->RecordCheckedWordStats(misspelled);
}
