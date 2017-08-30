// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_cookie_notifier.h"

#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

namespace {

void OnCookieChangedAsyncHelper(Profile* profile,
                                const net::CanonicalCookie& cookie,
                                bool removed,
                                net::CookieStore::ChangeCause cause) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Confirm the profile hasn't gone away since this object was created.
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  ChromeCookieDetails cookie_details(&cookie, removed, cause);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_COOKIE_CHANGED_FOR_EXTENSIONS,
      content::Source<Profile>(profile),
      content::Details<ChromeCookieDetails>(&cookie_details));
}

}  // namespace

ExtensionCookieNotifier::ExtensionCookieNotifier(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ExtensionCookieNotifier::AddStore(net::CookieStore* store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<net::CookieStore::CookieChangedSubscription> sub =
      store->AddCallbackForAllChanges(
          base::Bind(&ExtensionCookieNotifier::OnCookieChanged,
                     // |*store| is guaranteed to outlive this object, and
                     // this object's destruction will deregister the
                     // subscription.
                     base::Unretained(this)));

  subscriptions_.push_back(std::move(sub));
}

void ExtensionCookieNotifier::OnCookieChanged(
    const net::CanonicalCookie& cookie,
    net::CookieStore::ChangeCause cause) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&OnCookieChangedAsyncHelper, profile_, cookie,
                     !(cause == net::CookieStore::ChangeCause::INSERTED),
                     cause));
}

ExtensionCookieNotifier::~ExtensionCookieNotifier() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}
