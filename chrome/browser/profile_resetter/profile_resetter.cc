// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

ProfileResetter::ProfileResetter(Profile* profile)
    : profile_(profile),
      pending_reset_flags_(0) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

ProfileResetter::~ProfileResetter() {}

void ProfileResetter::Reset(ProfileResetter::ResettableFlags resettable_flags,
                            ExtensionHandling extension_handling,
                            const base::Closure& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

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

  DCHECK_EQ(resettable_flags, reset_triggered_for_flags);
}

void ProfileResetter::MarkAsDone(Resettable resettable) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Check that we are never called twice or unexpectedly.
  CHECK(pending_reset_flags_ & resettable);

  pending_reset_flags_ &= ~resettable;

  if (!pending_reset_flags_)
    callback_.Run();
}

void ProfileResetter::ResetDefaultSearchEngine() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  VLOG(1) << "ResetDefaultSearchEngine is not implemented, yet.";
  // TODO(battre/vabr): Implement
  MarkAsDone(DEFAULT_SEARCH_ENGINE);
}

void ProfileResetter::ResetHomepage() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  VLOG(1) << "ResetHomepage is not implemented, yet.";
  // TODO(battre/vabr): Implement
  MarkAsDone(HOMEPAGE);
}

void ProfileResetter::ResetContentSettings() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  VLOG(1) << "ResetContentSettings is not implemented, yet.";
  // TODO(battre/vabr): Implement
  MarkAsDone(CONTENT_SETTINGS);
}

void ProfileResetter::ResetCookiesAndSiteData() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  VLOG(1) << "ResetCookiesAndSiteData is not implemented, yet.";
  // TODO(battre/vabr): Implement
  MarkAsDone(COOKIES_AND_SITE_DATA);
}

void ProfileResetter::ResetExtensions(ExtensionHandling extension_handling) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  VLOG(1) << "ResetExtensions is not implemented, yet.";
  // TODO(battre/vabr): Implement
  MarkAsDone(EXTENSIONS);
}
