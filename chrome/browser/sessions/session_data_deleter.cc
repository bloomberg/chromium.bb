// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/url_request/url_request_context.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace {

void CookieDeleted(int num_cookies_deleted) {
  DCHECK_EQ(1, num_cookies_deleted);
}

class SessionDataDeleter
    : public base::RefCountedThreadSafe<SessionDataDeleter> {
 public:
  SessionDataDeleter(storage::SpecialStoragePolicy* storage_policy,
                     bool delete_only_by_session_only_policy);

  void Run(content::StoragePartition* storage_partition,
           ProfileIOData* profile_io_data);

 private:
  friend class base::RefCountedThreadSafe<SessionDataDeleter>;
  ~SessionDataDeleter();

  // Deletes the local storage described by |usages| for origins which are
  // session-only.
  void ClearSessionOnlyLocalStorage(
      content::StoragePartition* storage_partition,
      const std::vector<content::LocalStorageUsageInfo>& usages);

  // Deletes all cookies that are session only if
  // |delete_only_by_session_only_policy_| is false. Once completed or skipped,
  // this arranges for DeleteSessionOnlyOriginCookies to be called with a list
  // of all remaining cookies.
  void DeleteSessionCookiesOnIOThread(ProfileIOData* profile_io_data);

  // Called when all session-only cookies have been deleted.
  void DeleteSessionCookiesDone(int num_deleted);

  // Deletes the cookies in |cookies| that are for origins which are
  // session-only.
  void DeleteSessionOnlyOriginCookies(const net::CookieList& cookies);

  scoped_refptr<net::CookieStore> cookie_store_;
  scoped_refptr<storage::SpecialStoragePolicy> storage_policy_;
  const bool delete_only_by_session_only_policy_;

  DISALLOW_COPY_AND_ASSIGN(SessionDataDeleter);
};

SessionDataDeleter::SessionDataDeleter(
    storage::SpecialStoragePolicy* storage_policy,
    bool delete_only_by_session_only_policy)
    : storage_policy_(storage_policy),
      delete_only_by_session_only_policy_(delete_only_by_session_only_policy) {
}

void SessionDataDeleter::Run(content::StoragePartition* storage_partition,
                             ProfileIOData* profile_io_data) {
  if (storage_policy_.get() && storage_policy_->HasSessionOnlyOrigins()) {
    storage_partition->GetDOMStorageContext()->GetLocalStorageUsage(
        base::Bind(&SessionDataDeleter::ClearSessionOnlyLocalStorage,
                   this,
                   storage_partition));
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SessionDataDeleter::DeleteSessionCookiesOnIOThread,
                 this,
                 profile_io_data));
}

SessionDataDeleter::~SessionDataDeleter() {}

void SessionDataDeleter::ClearSessionOnlyLocalStorage(
    content::StoragePartition* storage_partition,
    const std::vector<content::LocalStorageUsageInfo>& usages) {
  DCHECK(storage_policy_.get());
  DCHECK(storage_policy_->HasSessionOnlyOrigins());
  for (size_t i = 0; i < usages.size(); ++i) {
    const content::LocalStorageUsageInfo& usage = usages[i];
    if (!storage_policy_->IsStorageSessionOnly(usage.origin))
      continue;
    storage_partition->GetDOMStorageContext()->DeleteLocalStorage(usage.origin);
  }
}

void SessionDataDeleter::DeleteSessionCookiesOnIOThread(
    ProfileIOData* profile_io_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::URLRequestContext* request_context =
      profile_io_data->GetMainRequestContext();
  cookie_store_ = request_context->cookie_store();
  if (delete_only_by_session_only_policy_) {
    cookie_store_->GetAllCookiesAsync(
        base::Bind(&SessionDataDeleter::DeleteSessionOnlyOriginCookies, this));
  } else {
    cookie_store_->DeleteSessionCookiesAsync(
        base::Bind(&SessionDataDeleter::DeleteSessionCookiesDone, this));
  }
}

void SessionDataDeleter::DeleteSessionCookiesDone(int num_deleted) {
  cookie_store_->GetAllCookiesAsync(
      base::Bind(&SessionDataDeleter::DeleteSessionOnlyOriginCookies, this));
}

void SessionDataDeleter::DeleteSessionOnlyOriginCookies(
    const net::CookieList& cookies) {
  if (!storage_policy_.get() || !storage_policy_->HasSessionOnlyOrigins())
    return;

  for (net::CookieList::const_iterator it = cookies.begin();
       it != cookies.end();
       ++it) {
    GURL url =
        net::cookie_util::CookieOriginToURL(it->Domain(), it->IsSecure());
    if (!storage_policy_->IsStorageSessionOnly(url))
      continue;
    cookie_store_->DeleteCanonicalCookieAsync(*it, base::Bind(CookieDeleted));
  }
}

}  // namespace

void DeleteSessionOnlyData(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (browser_shutdown::IsTryingToQuit())
    return;

  // TODO: Remove Athena special casing once the AthenaSessionRestore is in
  // place.
#if defined(OS_ANDROID)
  SessionStartupPref::Type startup_pref_type =
      SessionStartupPref::GetDefaultStartupType();
#else
  SessionStartupPref::Type startup_pref_type =
      StartupBrowserCreator::GetSessionStartupPref(
          *base::CommandLine::ForCurrentProcess(), profile).type;
#endif

  scoped_refptr<SessionDataDeleter> deleter(
      new SessionDataDeleter(profile->GetSpecialStoragePolicy(),
                             startup_pref_type == SessionStartupPref::LAST));
  deleter->Run(
      Profile::GetDefaultStoragePartition(profile),
      ProfileIOData::FromResourceContext(profile->GetResourceContext()));
}
