// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_message_filter.h"

#include "base/bind.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/render_process_host.h"
#include "net/url_request/url_fetcher.h"

using content::BrowserThread;

SpellCheckMessageFilter::SpellCheckMessageFilter(int render_process_id)
    : render_process_id_(render_process_id)
#if !defined(OS_MACOSX)
      ,
      route_id_(0),
      identifier_(0)
#endif
      {
}

void SpellCheckMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (message.type() == SpellCheckHostMsg_RequestDictionary::ID ||
      message.type() == SpellCheckHostMsg_NotifyChecked::ID)
    *thread = BrowserThread::UI;
#if !defined(OS_MACOSX)
  if (message.type() == SpellCheckHostMsg_CallSpellingService::ID)
    *thread = BrowserThread::UI;
#endif
}

bool SpellCheckMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SpellCheckMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_RequestDictionary,
                        OnSpellCheckerRequestDictionary)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_NotifyChecked,
                        OnNotifyChecked)
#if !defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_CallSpellingService,
                        OnCallSpellingService)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

SpellCheckMessageFilter::~SpellCheckMessageFilter() {}

void SpellCheckMessageFilter::OnSpellCheckerRequestDictionary() {
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  if (!host)
    return;  // Teardown.
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  // The renderer has requested that we initialize its spellchecker. This should
  // generally only be called once per session, as after the first call, all
  // future renderers will be passed the initialization information on startup
  // (or when the dictionary changes in some way).
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile);

  DCHECK(spellcheck_service);
  // The spellchecker initialization already started and finished; just send
  // it to the renderer.
  spellcheck_service->InitForRenderer(host);

  // TODO(rlp): Ensure that we do not initialize the hunspell dictionary more
  // than once if we get requests from different renderers.
}

void SpellCheckMessageFilter::OnNotifyChecked(const string16& word,
                                              bool misspelled) {
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  if (!host)
    return;  // Teardown.
  // Delegates to SpellCheckHost which tracks the stats of our spellchecker.
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile);
  DCHECK(spellcheck_service);
  if (spellcheck_service->GetMetrics())
    spellcheck_service->GetMetrics()->RecordCheckedWordStats(word, misspelled);
}

#if !defined(OS_MACOSX)
void SpellCheckMessageFilter::OnCallSpellingService(
    int route_id,
    int identifier,
    int document_tag,
    const string16& text) {
  DCHECK(!text.empty());
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!CallSpellingService(route_id, identifier, document_tag, text)) {
    std::vector<SpellCheckResult> results;
    Send(new SpellCheckMsg_RespondSpellingService(route_id,
                                                  identifier,
                                                  document_tag,
                                                  false,
                                                  text,
                                                  results));
    return;
  }
  route_id_ = route_id;
  identifier_ = identifier;
}

void SpellCheckMessageFilter::OnTextCheckComplete(
    int tag,
    bool success,
    const string16& text,
    const std::vector<SpellCheckResult>& results) {
  Send(new SpellCheckMsg_RespondSpellingService(route_id_,
                                                identifier_,
                                                tag,
                                                success,
                                                text,
                                                results));
  client_.reset();
}

bool SpellCheckMessageFilter::CallSpellingService(
    int route_id,
    int identifier,
    int document_tag,
    const string16& text) {
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  if (!host)
    return false;
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  if (!profile->GetPrefs()->GetBoolean(prefs::kSpellCheckUseSpellingService))
    return false;
  client_.reset(new SpellingServiceClient);
  return client_->RequestTextCheck(
    profile, SpellingServiceClient::SPELLCHECK, text,
    base::Bind(&SpellCheckMessageFilter::OnTextCheckComplete,
               base::Unretained(this), document_tag));
}
#endif
