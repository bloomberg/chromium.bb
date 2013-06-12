// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"

ProfileResetter::ProfileResetter(Profile* profile)
    : profile_(profile),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile_)),
      pending_reset_flags_(0) {
  DCHECK(CalledOnValidThread());
  DCHECK(profile_);
  registrar_.Add(this, chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
                 content::Source<TemplateURLService>(template_url_service_));
}

ProfileResetter::~ProfileResetter() {}

void ProfileResetter::Reset(ProfileResetter::ResettableFlags resettable_flags,
                            ExtensionHandling extension_handling,
                            const base::Closure& callback) {
  DCHECK(CalledOnValidThread());

  // We should never be called with unknown flags.
  CHECK_EQ(static_cast<ResettableFlags>(0), resettable_flags & ~ALL);

  // We should never be called when a previous reset has not finished.
  CHECK_EQ(static_cast<ResettableFlags>(0), pending_reset_flags_);

  callback_ = callback;

  // These flags are set to false by the individual reset functions.
  pending_reset_flags_ = resettable_flags;

  ResettableFlags reset_triggered_for_flags = 0;

  if (resettable_flags & DEFAULT_SEARCH_ENGINE) {
    reset_triggered_for_flags |= DEFAULT_SEARCH_ENGINE;
    ResetDefaultSearchEngine();
  }

  if (resettable_flags & HOMEPAGE) {
    reset_triggered_for_flags |= HOMEPAGE;
    ResetHomepage();
  }

  if (resettable_flags & CONTENT_SETTINGS) {
    reset_triggered_for_flags |= CONTENT_SETTINGS;
    ResetContentSettings();
  }

  if (resettable_flags & COOKIES_AND_SITE_DATA) {
    reset_triggered_for_flags |= COOKIES_AND_SITE_DATA;
    ResetCookiesAndSiteData();
  }

  if (resettable_flags & EXTENSIONS) {
    reset_triggered_for_flags |= EXTENSIONS;
    ResetExtensions(extension_handling);
  }

  if (resettable_flags & STARTUP_PAGE) {
    reset_triggered_for_flags |= STARTUP_PAGE;
    ResetStartPage();
  }

  DCHECK_EQ(resettable_flags, reset_triggered_for_flags);
}

bool ProfileResetter::IsActive() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return pending_reset_flags_ != 0;
}

void ProfileResetter::MarkAsDone(Resettable resettable) {
  DCHECK(CalledOnValidThread());

  // Check that we are never called twice or unexpectedly.
  CHECK(pending_reset_flags_ & resettable);

  pending_reset_flags_ &= ~resettable;

  if (!pending_reset_flags_) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     callback_);
    callback_.Reset();
  }
}

void ProfileResetter::ResetDefaultSearchEngine() {
  DCHECK(CalledOnValidThread());
  DCHECK(template_url_service_);

  // If TemplateURLServiceFactory is ready we can clean it right now.
  // Otherwise, load it and continue from ProfileResetter::Observe.
  if (template_url_service_->loaded()) {
    TemplateURLPrepopulateData::ClearPrepopulatedEnginesInPrefs(profile_);
    template_url_service_->ResetNonExtensionURLs();

    // Reset Google search URL.
    PrefService* prefs = profile_->GetPrefs();
    DCHECK(prefs);
    prefs->ClearPref(prefs::kLastPromptedGoogleURL);
    const TemplateURL* default_search_provider =
        template_url_service_->GetDefaultSearchProvider();
    if (default_search_provider &&
        default_search_provider->url_ref().HasGoogleBaseURLs())
      GoogleURLTracker::RequestServerCheck(profile_, true);

    MarkAsDone(DEFAULT_SEARCH_ENGINE);
  } else {
    template_url_service_->Load();
  }
}

void ProfileResetter::ResetHomepage() {
  DCHECK(CalledOnValidThread());
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  prefs->ClearPref(prefs::kHomePageIsNewTabPage);
  prefs->ClearPref(prefs::kHomePage);
  prefs->ClearPref(prefs::kShowHomeButton);
  MarkAsDone(HOMEPAGE);
}

void ProfileResetter::ResetContentSettings() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  // TODO(battre/vabr): Implement
  MarkAsDone(CONTENT_SETTINGS);
}

void ProfileResetter::ResetCookiesAndSiteData() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  // TODO(battre/vabr): Implement
  MarkAsDone(COOKIES_AND_SITE_DATA);
}

void ProfileResetter::ResetExtensions(ExtensionHandling extension_handling) {
  DCHECK(CalledOnValidThread());
  ExtensionService* extension_service = profile_->GetExtensionService();
  DCHECK(extension_service);
  extension_service->DisableUserExtensions();

  MarkAsDone(EXTENSIONS);
}

void ProfileResetter::ResetStartPage() {
  DCHECK(CalledOnValidThread());
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  prefs->ClearPref(prefs::kRestoreOnStartup);
  prefs->ClearPref(prefs::kURLsToRestoreOnStartup);
  prefs->SetBoolean(prefs::kRestoreOnStartupMigrated, true);
  MarkAsDone(STARTUP_PAGE);
}

void ProfileResetter::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
  // TemplateURLService has loaded. If we need to clean search engines, it's
  // time to go on.
  if (pending_reset_flags_ & DEFAULT_SEARCH_ENGINE)
    ResetDefaultSearchEngine();
}
