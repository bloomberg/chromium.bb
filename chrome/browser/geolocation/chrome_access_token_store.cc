// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/chrome_access_token_store.h"

#include "base/bind.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::AccessTokenStore;
using content::BrowserThread;

namespace {

bool IsUnsupportedNetworkProviderUrl(const GURL& url) {
  const std::string& spec = url.spec();

  // Unsupported after Chrome v14.
  if (spec == "https://www.google.com/loc/json")
    return true;

  // Unsupported after Chrome v22.
  if (spec == "https://maps.googleapis.com/maps/api/browserlocation/json")
    return true;

  return false;
}

// Loads access tokens and other necessary data on the UI thread, and
// calls back to the originator on the originating thread.
class TokenLoadingJob : public base::RefCountedThreadSafe<TokenLoadingJob> {
 public:
  TokenLoadingJob(
      const AccessTokenStore::LoadAccessTokensCallbackType& callback)
      : callback_(callback),
        system_request_context_(NULL) {
  }

  void Run() {
    BrowserThread::PostTaskAndReply(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&TokenLoadingJob::PerformWorkOnUIThread, this),
        base::Bind(&TokenLoadingJob::RespondOnOriginatingThread, this));
  }

 private:
  friend class base::RefCountedThreadSafe<TokenLoadingJob>;

  ~TokenLoadingJob() {}

  void PerformWorkOnUIThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DictionaryPrefUpdate update(g_browser_process->local_state(),
                                prefs::kGeolocationAccessToken);
    base::DictionaryValue* token_dictionary = update.Get();

    std::vector<std::string> providers_to_remove;
    // The dictionary value could be NULL if the pref has never been set.
    if (token_dictionary != NULL) {
      for (base::DictionaryValue::Iterator it(*token_dictionary); !it.IsAtEnd();
           it.Advance()) {
        GURL url(it.key());
        if (!url.is_valid())
          continue;
        if (IsUnsupportedNetworkProviderUrl(url)) {
          providers_to_remove.push_back(it.key());
          continue;
        }
        it.value().GetAsString(&access_token_set_[url]);
      }
      for (size_t i = 0; i < providers_to_remove.size(); ++i) {
        token_dictionary->RemoveWithoutPathExpansion(
            providers_to_remove[i], NULL);
      }
    }

    system_request_context_ = g_browser_process->system_request_context();
  }

  void RespondOnOriginatingThread() {
    callback_.Run(access_token_set_, system_request_context_);
  }

  AccessTokenStore::LoadAccessTokensCallbackType callback_;
  AccessTokenStore::AccessTokenSet access_token_set_;
  net::URLRequestContextGetter* system_request_context_;
};

}  // namespace

void ChromeAccessTokenStore::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kGeolocationAccessToken);
}

ChromeAccessTokenStore::ChromeAccessTokenStore() {}

void ChromeAccessTokenStore::LoadAccessTokens(
    const LoadAccessTokensCallbackType& callback) {
  scoped_refptr<TokenLoadingJob> job(new TokenLoadingJob(callback));
  job->Run();
}

ChromeAccessTokenStore::~ChromeAccessTokenStore() {}

static void SetAccessTokenOnUIThread(const GURL& server_url,
                                     const base::string16& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              prefs::kGeolocationAccessToken);
  base::DictionaryValue* access_token_dictionary = update.Get();
  access_token_dictionary->SetWithoutPathExpansion(
      server_url.spec(), new base::StringValue(token));
}

void ChromeAccessTokenStore::SaveAccessToken(
    const GURL& server_url,
    const base::string16& access_token) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SetAccessTokenOnUIThread, server_url, access_token));
}
