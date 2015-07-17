// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_message_filter_platform.h"

#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

SpellCheckMessageFilterPlatform::SpellCheckMessageFilterPlatform(
    int render_process_id)
    : BrowserMessageFilter(SpellCheckMsgStart),
      render_process_id_(render_process_id) {
}

void SpellCheckMessageFilterPlatform::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
}

bool SpellCheckMessageFilterPlatform::OnMessageReceived(
    const IPC::Message& message) {
    return true;
}

// static
void SpellCheckMessageFilterPlatform::CombineResults(
    std::vector<SpellCheckResult>* remote_results,
    const std::vector<SpellCheckResult>& local_results) {
}

SpellCheckMessageFilterPlatform::~SpellCheckMessageFilterPlatform() {}

void SpellCheckMessageFilterPlatform::OnCheckSpelling(
    const base::string16& word,
    int route_id,
    bool* correct) {
}

void SpellCheckMessageFilterPlatform::OnFillSuggestionList(
    const base::string16& word,
    std::vector<base::string16>* suggestions) {
}

void SpellCheckMessageFilterPlatform::OnShowSpellingPanel(bool show) {
}

void SpellCheckMessageFilterPlatform::OnUpdateSpellingPanelWithMisspelledWord(
    const base::string16& word) {
}

void SpellCheckMessageFilterPlatform::OnRequestTextCheck(
    int route_id,
    int identifier,
    const base::string16& text,
    std::vector<SpellCheckMarker> markers) {
}

int SpellCheckMessageFilterPlatform::ToDocumentTag(int route_id) {
    NOTREACHED();
    return -1;
}

void SpellCheckMessageFilterPlatform::RetireDocumentTag(int route_id) {
    NOTREACHED();
}
