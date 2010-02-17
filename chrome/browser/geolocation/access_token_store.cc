// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/access_token_store.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"

namespace {
class ChromePrefsAccessTokenStore : public AccessTokenStore {
 public:
  ChromePrefsAccessTokenStore(
      const GURL& url, bool token_valid, const string16& initial_access_token);

 private:
  // AccessTokenStore
  virtual void DoSetAccessToken(const string16& access_token);
};


class ChromePrefsAccessTokenStoreFactory : public AccessTokenStoreFactory {
 private:
  void LoadDictionaryStoreInUIThread(ChromeThread::ID client_thread_id,
                                     const GURL& default_url);
  void NotifyDelegateInClientThread(const TokenStoreSet& token_stores);

  // AccessTokenStoreFactory
  virtual void CreateAccessTokenStores(
      const base::WeakPtr<AccessTokenStoreFactory::Delegate>& delegate,
      const GURL& default_url);

  base::WeakPtr<AccessTokenStoreFactory::Delegate> delegate_;
};

ChromePrefsAccessTokenStore::ChromePrefsAccessTokenStore(
      const GURL& url, bool token_valid, const string16& initial_access_token)
    : AccessTokenStore(url, token_valid, initial_access_token) {
}

void ChromePrefsAccessTokenStore::DoSetAccessToken(
    const string16& access_token) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE, NewRunnableMethod(
            this, &ChromePrefsAccessTokenStore::DoSetAccessToken,
            access_token));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DictionaryValue* access_token_dictionary =
      g_browser_process->local_state()->GetMutableDictionary(
          prefs::kGeolocationAccessToken);
  access_token_dictionary->SetWithoutPathExpansion(
      UTF8ToWide(server_url().spec()),
      Value::CreateStringValueFromUTF16(access_token));
}

void ChromePrefsAccessTokenStoreFactory::LoadDictionaryStoreInUIThread(
    ChromeThread::ID client_thread_id, const GURL& default_url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  const DictionaryValue* token_dictionary =
      g_browser_process->local_state()->GetDictionary(
          prefs::kGeolocationAccessToken);
  TokenStoreSet token_stores;
  // Careful: The returned value could be NULL if the pref has never been set.
  if (token_dictionary != NULL) {
    for (DictionaryValue::key_iterator it = token_dictionary->begin_keys();
        it != token_dictionary->end_keys(); ++it) {
      GURL this_url(WideToUTF8(*it));
      if (!this_url.is_valid())
        continue;
      string16 token;
      const bool token_valid =
          token_dictionary->GetStringAsUTF16WithoutPathExpansion(*it, &token);
      token_stores[this_url] =
          new ChromePrefsAccessTokenStore(this_url, token_valid, token);
    }
  }
  if (default_url.is_valid() && token_stores[default_url] == NULL) {
    token_stores[default_url] =
        new ChromePrefsAccessTokenStore(default_url, false, string16());
  }
  ChromeThread::PostTask(
      client_thread_id, FROM_HERE, NewRunnableMethod(
          this,
          &ChromePrefsAccessTokenStoreFactory::NotifyDelegateInClientThread,
          token_stores));
}

void ChromePrefsAccessTokenStoreFactory::NotifyDelegateInClientThread(
    const TokenStoreSet& token_stores) {
  if (delegate_ != NULL) {
    delegate_->OnAccessTokenStoresCreated(token_stores);
  }
}

void ChromePrefsAccessTokenStoreFactory::CreateAccessTokenStores(
    const base::WeakPtr<AccessTokenStoreFactory::Delegate>& delegate,
    const GURL& default_url) {
  // Capture the thread that created the factory, in order to make callbacks
  // on the same thread. We could capture a MessageLoop* but in practice we only
  // expect to be called from well-known chrome threads, so this is sufficient.
  ChromeThread::ID client_thread_id;
  bool ok = ChromeThread::GetCurrentThreadIdentifier(&client_thread_id);
  CHECK(ok);
  DCHECK_NE(ChromeThread::UI, client_thread_id)
      << "If I'm only used from the UI thread I don't need to post tasks";
  delegate_ = delegate;
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE, NewRunnableMethod(
          this,
          &ChromePrefsAccessTokenStoreFactory::LoadDictionaryStoreInUIThread,
          client_thread_id, default_url));
}
}  // namespace

void AccessTokenStore::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kGeolocationAccessToken);
}

AccessTokenStore::AccessTokenStore(
      const GURL& url, bool token_valid, const string16& initial_access_token)
    : url_(url), access_token_valid_(token_valid),
      access_token_(initial_access_token) {
}

AccessTokenStore::~AccessTokenStore() {
}

GURL AccessTokenStore::server_url() const {
  return url_;
}

void AccessTokenStore::SetAccessToken(const string16& access_token) {
  access_token_ = access_token;
  access_token_valid_ = true;
  DoSetAccessToken(access_token);
}

bool AccessTokenStore::GetAccessToken(string16* access_token) const {
  DCHECK(access_token);
  *access_token = access_token_;
  return access_token_valid_;
}

AccessTokenStoreFactory::~AccessTokenStoreFactory() {
}

// Creates a new access token store backed by the global chome prefs.
AccessTokenStoreFactory* NewChromePrefsAccessTokenStoreFactory() {
  return new ChromePrefsAccessTokenStoreFactory;
}
