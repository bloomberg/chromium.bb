// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist.h"

#include <algorithm>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace extensions {

namespace {

// The safe browsing database manager to use. Make this a global/static variable
// rather than a member of Blacklist because Blacklist accesses the real
// database manager before it has a chance to get a fake one.
class LazySafeBrowsingDatabaseManager {
 public:
  LazySafeBrowsingDatabaseManager() {
#if defined(FULL_SAFE_BROWSING) || defined(MOBILE_SAFE_BROWSING)
    if (g_browser_process && g_browser_process->safe_browsing_service()) {
      instance_ =
          g_browser_process->safe_browsing_service()->database_manager();
    }
#endif
  }

  scoped_refptr<SafeBrowsingDatabaseManager> get() {
    return instance_;
  }

  void set(scoped_refptr<SafeBrowsingDatabaseManager> instance) {
    instance_ = instance;
  }

 private:
  scoped_refptr<SafeBrowsingDatabaseManager> instance_;
};

static base::LazyInstance<LazySafeBrowsingDatabaseManager> g_database_manager =
    LAZY_INSTANCE_INITIALIZER;

// Implementation of SafeBrowsingDatabaseManager::Client, the class which is
// called back from safebrowsing queries.
//
// Constructed on any thread but lives on the IO from then on.
class SafeBrowsingClientImpl
    : public SafeBrowsingDatabaseManager::Client,
      public base::RefCountedThreadSafe<SafeBrowsingClientImpl> {
 public:
  typedef base::Callback<void(const std::set<std::string>&)> OnResultCallback;

  // Constructs a client to query the database manager for |extension_ids| and
  // run |callback| with the IDs of those which have been blacklisted.
  SafeBrowsingClientImpl(
      const std::set<std::string>& extension_ids,
      const OnResultCallback& callback)
      : callback_message_loop_(base::MessageLoopProxy::current()),
        callback_(callback) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&SafeBrowsingClientImpl::StartCheck, this,
                   g_database_manager.Get().get(),
                   extension_ids));
  }

 private:
  friend class base::RefCountedThreadSafe<SafeBrowsingClientImpl>;
  virtual ~SafeBrowsingClientImpl() {}

  // Pass |database_manager| as a parameter to avoid touching
  // SafeBrowsingService on the IO thread.
  void StartCheck(scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
                  const std::set<std::string>& extension_ids) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (database_manager->CheckExtensionIDs(extension_ids, this)) {
      // Definitely not blacklisted. Callback immediately.
      callback_message_loop_->PostTask(
          FROM_HERE,
          base::Bind(callback_, std::set<std::string>()));
      return;
    }
    // Something might be blacklisted, response will come in
    // OnCheckExtensionsResult.
    AddRef();  // Balanced in OnCheckExtensionsResult
  }

  virtual void OnCheckExtensionsResult(
      const std::set<std::string>& hits) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    callback_message_loop_->PostTask(FROM_HERE, base::Bind(callback_, hits));
    Release();  // Balanced in StartCheck.
  }

  scoped_refptr<base::MessageLoopProxy> callback_message_loop_;
  OnResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingClientImpl);
};

void IsNotEmpty(const Blacklist::IsBlacklistedCallback& callback,
                const std::set<std::string>& set) {
  callback.Run(!set.empty());
}

}  // namespace

Blacklist::Observer::Observer(Blacklist* blacklist) : blacklist_(blacklist) {
  blacklist_->AddObserver(this);
}

Blacklist::Observer::~Observer() {
  blacklist_->RemoveObserver(this);
}

Blacklist::ScopedDatabaseManagerForTest::ScopedDatabaseManagerForTest(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager)
    : original_(GetDatabaseManager()) {
  SetDatabaseManager(database_manager);
}

Blacklist::ScopedDatabaseManagerForTest::~ScopedDatabaseManagerForTest() {
  SetDatabaseManager(original_);
}

Blacklist::Blacklist(ExtensionPrefs* prefs) : prefs_(prefs) {
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager =
      g_database_manager.Get().get();
  if (database_manager) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_SAFE_BROWSING_UPDATE_COMPLETE,
                   content::Source<SafeBrowsingDatabaseManager>(
                        database_manager));
  }

  // TODO(kalman): Delete anything from the pref blacklist that is in the
  // safebrowsing blacklist (of course, only entries for which the extension
  // hasn't been installed).
  //
  // Or maybe just wait until we're able to delete the pref blacklist
  // altogether (when we're sure it's a strict subset of the safebrowsing one).
}

Blacklist::~Blacklist() {
}

void Blacklist::GetBlacklistedIDs(const std::set<std::string>& ids,
                                  const GetBlacklistedIDsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (ids.empty()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, std::set<std::string>()));
    return;
  }

  // The blacklisted IDs are the union of those blacklisted in prefs and
  // those blacklisted from safe browsing.
  std::set<std::string> pref_blacklisted_ids;
  for (std::set<std::string>::const_iterator it = ids.begin();
       it != ids.end(); ++it) {
    if (prefs_->IsExtensionBlacklisted(*it))
      pref_blacklisted_ids.insert(*it);
  }

  if (!g_database_manager.Get().get()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, pref_blacklisted_ids));
    return;
  }

  // Constructing the SafeBrowsingClientImpl begins the process of asking
  // safebrowsing for the blacklisted extensions.
  new SafeBrowsingClientImpl(
      ids,
      base::Bind(&Blacklist::OnSafeBrowsingResponse, AsWeakPtr(),
                 pref_blacklisted_ids,
                 callback));
}

void Blacklist::IsBlacklisted(const std::string& extension_id,
                              const IsBlacklistedCallback& callback) {
  std::set<std::string> check;
  check.insert(extension_id);
  GetBlacklistedIDs(check, base::Bind(&IsNotEmpty, callback));
}

void Blacklist::SetFromUpdater(const std::vector<std::string>& ids,
                               const std::string& version) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::set<std::string> ids_as_set;
  for (std::vector<std::string>::const_iterator it = ids.begin();
       it != ids.end(); ++it) {
    if (Extension::IdIsValid(*it))
      ids_as_set.insert(*it);
    else
      LOG(WARNING) << "Got invalid extension ID \"" << *it << "\"";
  }

  std::set<std::string> from_prefs = prefs_->GetBlacklistedExtensions();

  std::set<std::string> no_longer_blacklisted;
  std::set_difference(from_prefs.begin(), from_prefs.end(),
                      ids_as_set.begin(), ids_as_set.end(),
                      std::inserter(no_longer_blacklisted,
                                    no_longer_blacklisted.begin()));
  std::set<std::string> not_yet_blacklisted;
  std::set_difference(ids_as_set.begin(), ids_as_set.end(),
                      from_prefs.begin(), from_prefs.end(),
                      std::inserter(not_yet_blacklisted,
                                    not_yet_blacklisted.begin()));

  for (std::set<std::string>::iterator it = no_longer_blacklisted.begin();
       it != no_longer_blacklisted.end(); ++it) {
    prefs_->SetExtensionBlacklisted(*it, false);
  }
  for (std::set<std::string>::iterator it = not_yet_blacklisted.begin();
       it != not_yet_blacklisted.end(); ++it) {
    prefs_->SetExtensionBlacklisted(*it, true);
  }

  prefs_->pref_service()->SetString(prefs::kExtensionBlacklistUpdateVersion,
                                    version);

  FOR_EACH_OBSERVER(Observer, observers_, OnBlacklistUpdated());
}

void Blacklist::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void Blacklist::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

// static
void Blacklist::SetDatabaseManager(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager) {
  g_database_manager.Get().set(database_manager);
}

// static
scoped_refptr<SafeBrowsingDatabaseManager> Blacklist::GetDatabaseManager() {
  return g_database_manager.Get().get();
}

void Blacklist::OnSafeBrowsingResponse(
    const std::set<std::string>& pref_blacklisted_ids,
    const GetBlacklistedIDsCallback& callback,
    const std::set<std::string>& safebrowsing_blacklisted_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::set<std::string> blacklist = pref_blacklisted_ids;
  blacklist.insert(safebrowsing_blacklisted_ids.begin(),
                   safebrowsing_blacklisted_ids.end());

  callback.Run(blacklist);
}

void Blacklist::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_SAFE_BROWSING_UPDATE_COMPLETE, type);
  FOR_EACH_OBSERVER(Observer, observers_, OnBlacklistUpdated());
}

}  // namespace extensions
