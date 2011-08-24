// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_profile.h"

#include "base/lazy_instance.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/common/pref_names.h"

namespace {
base::LazyInstance<SpellCheckProfile::CustomWordList> g_empty_list(
    base::LINKER_INITIALIZED);
}  // namespace

SpellCheckProfile::SpellCheckProfile()
    : host_ready_(false) {
}

SpellCheckProfile::~SpellCheckProfile() {
  if (host_.get())
    host_->UnsetProfile();
}

SpellCheckHost* SpellCheckProfile::GetHost() {
  return host_ready_ ? host_.get() : NULL;
}

SpellCheckProfile::ReinitializeResult SpellCheckProfile::ReinitializeHost(
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
    host_ = NULL;
    host_deleted = true;
  }

  if (enable) {
    // Retrieve the (perhaps updated recently) dictionary name from preferences.
    host_ = CreateHost(this, language, request_context, metrics_.get());
    return REINITIALIZE_CREATED_HOST;
  }

  return host_deleted ? REINITIALIZE_REMOVED_HOST : REINITIALIZE_DID_NOTHING;
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

void SpellCheckProfile::SpellCheckHostInitialized(
    SpellCheckProfile::CustomWordList* custom_words) {
  host_ready_ = !!host_.get() && host_->IsReady();
  custom_words_.reset(custom_words);
  if (metrics_.get()) {
    int count = custom_words_.get() ? custom_words_->size() : 0;
    metrics_->RecordCustomWordCountStats(count);
  }
}

const SpellCheckProfile::CustomWordList&
SpellCheckProfile::GetCustomWords() const {
  return custom_words_.get() ? *custom_words_ : g_empty_list.Get();
}

void SpellCheckProfile::CustomWordAddedLocally(const std::string& word) {
  if (!custom_words_.get())
    custom_words_.reset(new CustomWordList());
  custom_words_->push_back(word);
  if (metrics_.get())
    metrics_->RecordCustomWordCountStats(custom_words_->size());
}
