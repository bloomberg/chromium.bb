// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_profile.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/common/pref_names.h"

SpellCheckProfile::SpellCheckProfile()
    : host_ready_(false) {
}

SpellCheckProfile::~SpellCheckProfile() {
  if (host_.get())
    host_->UnsetObserver();
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
    host_->UnsetObserver();
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
    SpellCheckHostObserver* observer,
    const std::string& language,
    net::URLRequestContextGetter* request_context,
    SpellCheckHostMetrics* metrics) {
  return SpellCheckHost::Create(
      observer, language, request_context, metrics);
}

bool SpellCheckProfile::IsTesting() const {
  return false;
}

void SpellCheckProfile::StartRecordingMetrics(bool spellcheck_enabled) {
  metrics_.reset(new SpellCheckHostMetrics());
  metrics_->RecordEnabledStats(spellcheck_enabled);
}

void SpellCheckProfile::SpellCheckHostInitialized() {
  host_ready_ = !!host_.get() && host_->IsReady();
}
