// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_message_filter_mac.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_platform_mac.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

namespace {

bool CompareLocation(const SpellCheckResult& r1,
                     const SpellCheckResult& r2) {
  return r1.location < r2.location;
}

}  // namespace

class SpellingRequest {
 public:
  SpellingRequest(SpellingServiceClient* client,
                  content::BrowserMessageFilter* destination,
                  int render_process_id);

  void RequestCheck(const string16& text,
                    int route_id,
                    int identifier,
                    int document_tag);
 private:
  // Request server-side checking.
  void RequestRemoteCheck(const string16& text);

  // Request a check from local spell checker.
  void RequestLocalCheck(const string16& text, int document_tag);

  // Check if all pending requests are done, send reply to render process if so.
  void OnCheckCompleted();

  // Called when server-side checking is complete.
  void OnRemoteCheckCompleted(bool success,
                              const string16& text,
                              const std::vector<SpellCheckResult>& results);

  // Called when local checking is complete.
  void OnLocalCheckCompleted(const std::vector<SpellCheckResult>& results);

  std::vector<SpellCheckResult> local_results_;
  std::vector<SpellCheckResult> remote_results_;

  bool local_pending_;
  bool remote_pending_;
  bool remote_success_;

  SpellingServiceClient* client_;  // Owned by |destination|.
  content::BrowserMessageFilter* destination_;  // ref-counted.
  int render_process_id_;

  int route_id_;
  int identifier_;
  int document_tag_;
};

SpellingRequest::SpellingRequest(SpellingServiceClient* client,
                                 content::BrowserMessageFilter* destination,
                                 int render_process_id)
    : local_pending_(true),
      remote_pending_(true),
      client_(client),
      destination_(destination),
      render_process_id_(render_process_id),
      route_id_(-1),
      identifier_(-1),
      document_tag_(-1) {
  destination_->AddRef();
}

void SpellingRequest::RequestCheck(const string16& text,
                                   int route_id,
                                   int identifier,
                                   int document_tag) {
  DCHECK(!text.empty());
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  route_id_ = route_id;
  identifier_ = identifier;
  document_tag_ = document_tag;

  // Send the remote query out.
  RequestRemoteCheck(text);
  RequestLocalCheck(text, document_tag_);
}

void SpellingRequest::RequestRemoteCheck(const string16& text) {
  Profile* profile = NULL;
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  if (host)
    profile = Profile::FromBrowserContext(host->GetBrowserContext());

  client_->RequestTextCheck(
    profile,
    SpellingServiceClient::SPELLCHECK,
    text,
    base::Bind(&SpellingRequest::OnRemoteCheckCompleted,
               base::Unretained(this)));
}

void SpellingRequest::RequestLocalCheck(const string16& text,
                                        int document_tag) {
  spellcheck_mac::RequestTextCheck(
      document_tag,
      text,
      base::Bind(&SpellingRequest::OnLocalCheckCompleted,
                 base::Unretained(this)));
}

void SpellingRequest::OnCheckCompleted() {
  // Final completion can happen on any thread - don't DCHECK thread.

  if (local_pending_ || remote_pending_)
    return;

  const std::vector<SpellCheckResult>* check_results = &local_results_;
  if (remote_success_) {
    std::sort(remote_results_.begin(), remote_results_.end(), CompareLocation);
    std::sort(local_results_.begin(), local_results_.end(), CompareLocation);
    SpellCheckMessageFilterMac::CombineResults(&remote_results_,
                                               local_results_);
    check_results = &remote_results_;
  }

  destination_->Send(
      new SpellCheckMsg_RespondTextCheck(
          route_id_,
          identifier_,
          *check_results));
  destination_->Release();

  // Object is self-managed - at this point, its life span is over.
  delete this;
}

void SpellingRequest::OnRemoteCheckCompleted(
    bool success,
    const string16& text,
    const std::vector<SpellCheckResult>& results) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  remote_success_ = success;
  remote_results_ = results;
  remote_pending_ = false;

  OnCheckCompleted();
}

void SpellingRequest::OnLocalCheckCompleted(
    const std::vector<SpellCheckResult>& results) {
  // Local checking can happen on any thread - don't DCHECK thread.

  local_results_ = results;
  local_pending_ = false;

  OnCheckCompleted();
}


SpellCheckMessageFilterMac::SpellCheckMessageFilterMac(int render_process_id)
    : render_process_id_(render_process_id),
      client_(new SpellingServiceClient) {
}

void SpellCheckMessageFilterMac::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (message.type() == SpellCheckHostMsg_RequestTextCheck::ID)
    *thread = BrowserThread::UI;
}

bool SpellCheckMessageFilterMac::OnMessageReceived(const IPC::Message& message,
                                                   bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SpellCheckMessageFilterMac, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_CheckSpelling,
                        OnCheckSpelling)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_FillSuggestionList,
                        OnFillSuggestionList)
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

// static
void SpellCheckMessageFilterMac::CombineResults(
    std::vector<SpellCheckResult>* remote_results,
    const std::vector<SpellCheckResult>& local_results) {
  std::vector<SpellCheckResult>::const_iterator local_iter(
      local_results.begin());
  std::vector<SpellCheckResult>::iterator remote_iter;

  for (remote_iter = remote_results->begin();
       remote_iter != remote_results->end();
       ++remote_iter) {
    // Discard all local results occurring before remote result.
    while (local_iter != local_results.end() &&
           local_iter->location < remote_iter->location) {
      local_iter++;
    }

    // Unless local and remote result coincide, result is GRAMMAR.
    remote_iter->type = SpellCheckResult::GRAMMAR;
    if (local_iter != local_results.end() &&
        local_iter->location == remote_iter->location &&
        local_iter->length == remote_iter->length) {
      remote_iter->type = SpellCheckResult::SPELLING;
    }
  }
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
  // SpellingRequest self-destructs.
  SpellingRequest* request =
    new SpellingRequest(client_.get(), this, render_process_id_);
  request->RequestCheck(text, route_id, identifier, ToDocumentTag(route_id));
}

int SpellCheckMessageFilterMac::ToDocumentTag(int route_id) {
  if (!tag_map_.count(route_id))
    tag_map_[route_id] = spellcheck_mac::GetDocumentTag();
  return tag_map_[route_id];
}

// TODO(groby): We are currently not notified of retired tags. We need
// to track destruction of RenderViewHosts on the browser process side
// to update our mappings when a document goes away.
void SpellCheckMessageFilterMac::RetireDocumentTag(int route_id) {
  spellcheck_mac::CloseDocumentWithTag(ToDocumentTag(route_id));
  tag_map_.erase(route_id);
}

