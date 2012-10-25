// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_profile.h"

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;
using chrome::spellcheck_common::WordList;

SpellCheckProfile::SpellCheckProfile(Profile* profile)
    : profile_(profile),
      host_ready_(false),
      profile_dir_(profile->GetPath()),
      custom_dictionary_(new SpellcheckCustomDictionary(profile)) {
  PrefService* prefs = profile_->GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kSpellCheckDictionary, this);
  pref_change_registrar_.Add(prefs::kEnableSpellCheck, this);
  pref_change_registrar_.Add(prefs::kEnableAutoSpellCorrect, this);
}

SpellCheckProfile::~SpellCheckProfile() {
  // Remove pref observers
  pref_change_registrar_.RemoveAll();
}

SpellCheckHost* SpellCheckProfile::GetHost() {
  return host_ready_ ? host_.get() : NULL;
}

void SpellCheckProfile::ReinitializeSpellCheckHost(bool force) {
  PrefService* pref = profile_->GetPrefs();
  SpellCheckProfile::ReinitializeResult result = ReinitializeHostImpl(
      force,
      pref->GetBoolean(prefs::kEnableSpellCheck),
      pref->GetString(prefs::kSpellCheckDictionary),
      profile_->GetRequestContext());
  if (result == SpellCheckProfile::REINITIALIZE_REMOVED_HOST) {
    // The spellchecker has been disabled.
    for (content::RenderProcessHost::iterator i(
            content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      content::RenderProcessHost* process = i.GetCurrentValue();
      process->Send(new SpellCheckMsg_Init(IPC::InvalidPlatformFileForTransit(),
                                           std::vector<std::string>(),
                                           std::string(),
                                           false));
    }
  }
}

SpellCheckHost* SpellCheckProfile::CreateHost(
    SpellCheckProfileProvider* provider,
    const std::string& language,
    net::URLRequestContextGetter* request_context,
    SpellCheckHostMetrics* metrics) {
  return SpellCheckHost::Create(
      provider, language, request_context, metrics);
}

bool SpellCheckProfile::IsTesting() const {
  return false;
}

void SpellCheckProfile::StartRecordingMetrics(bool spellcheck_enabled) {
  metrics_.reset(new SpellCheckHostMetrics());
  metrics_->RecordEnabledStats(spellcheck_enabled);
}

void SpellCheckProfile::SpellCheckHostInitialized(WordList* custom_words) {
  host_ready_ = !!host_.get() && host_->IsReady();
  custom_dictionary_->SetCustomWordList(custom_words);
}

const WordList& SpellCheckProfile::GetCustomWords() const {
  return custom_dictionary_->GetCustomWords();
}

void SpellCheckProfile::CustomWordAddedLocally(const std::string& word) {
  custom_dictionary_->CustomWordAddedLocally(word);
}

void SpellCheckProfile::LoadCustomDictionary(WordList* custom_words) {
  custom_dictionary_->LoadDictionaryIntoCustomWordList(*custom_words);
}

void SpellCheckProfile::WriteWordToCustomDictionary(const std::string& word) {
  custom_dictionary_->WriteWordToCustomDictionary(word);
}

void SpellCheckProfile::Shutdown() {
  if (host_.get())
    host_->UnsetProfile();

  profile_ = NULL;
}

void SpellCheckProfile::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name_in = content::Details<std::string>(details).ptr();
      PrefService* prefs = content::Source<PrefService>(source).ptr();
      DCHECK(pref_name_in && prefs);
      if (*pref_name_in == prefs::kSpellCheckDictionary ||
          *pref_name_in == prefs::kEnableSpellCheck) {
        ReinitializeSpellCheckHost(true);
      } else if (*pref_name_in == prefs::kEnableAutoSpellCorrect) {
        bool enabled = prefs->GetBoolean(prefs::kEnableAutoSpellCorrect);
        for (content::RenderProcessHost::iterator i(
                content::RenderProcessHost::AllHostsIterator());
             !i.IsAtEnd(); i.Advance()) {
          content::RenderProcessHost* process = i.GetCurrentValue();
          process->Send(new SpellCheckMsg_EnableAutoSpellCorrect(enabled));
        }
      }
    }
  }
}

SpellCheckProfile::ReinitializeResult SpellCheckProfile::ReinitializeHostImpl(
    bool force,
    bool enable,
    const std::string& language,
    net::URLRequestContextGetter* request_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) || IsTesting());
  // If we are already loading the spellchecker, and this is just a hint to
  // load the spellchecker, do nothing.
  if (!force && host_.get())
    return REINITIALIZE_DID_NOTHING;

  host_ready_ = false;

  bool host_deleted = false;
  if (host_.get()) {
    host_->UnsetProfile();
    host_.reset(NULL);
    host_deleted = true;
  }

  if (enable) {
    // Retrieve the (perhaps updated recently) dictionary name from preferences.
    host_.reset(CreateHost(this, language, request_context, metrics_.get()));
    return REINITIALIZE_CREATED_HOST;
  }

  return host_deleted ? REINITIALIZE_REMOVED_HOST : REINITIALIZE_DID_NOTHING;
}

