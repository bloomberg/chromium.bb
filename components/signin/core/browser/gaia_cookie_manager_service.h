// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_GAIA_COOKIE_MANAGER_SERVICE_H
#define COMPONENTS_SIGNIN_CORE_BROWSER_GAIA_COOKIE_MANAGER_SERVICE_H

#include <deque>

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/signin/core/browser/signin_client.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/ubertoken_fetcher.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_fetcher_delegate.h"

class GaiaAuthFetcher;
class GoogleServiceAuthError;
class OAuth2TokenService;

namespace net {
class URLFetcher;
}

// Merges a Google account known to Chrome into the cookie jar.  When merging
// multiple accounts, one instance of the helper is better than multiple
// instances if there is the possibility that they run concurrently, since
// changes to the cookie must be serialized.
//
// Also checks the External CC result to ensure no services that consume the
// GAIA cookie are blocked (such as youtube). This is executed once for the
// lifetime of this object, when the first call is made to AddAccountToCookie.
class GaiaCookieManagerService : public KeyedService,
                                 public GaiaAuthConsumer,
                                 public UbertokenConsumer,
                                 public net::URLFetcherDelegate {
 public:
  class Observer {
   public:
    // Called whenever a merge session is completed.  The account that was
    // merged is given by |account_id|.  If |error| is equal to
    // GoogleServiceAuthError::AuthErrorNone() then the merge succeeeded.
    virtual void OnAddAccountToCookieCompleted(
        const std::string& account_id,
        const GoogleServiceAuthError& error) = 0;

    // Called when ExternalCcResultFetcher completes. From this moment
    // forward calls to AddAccountToCookie() will use the result in merge
    // session calls. If |succeeded| is false, not all connections were checked,
    // but some may have been. AddAccountToCookie() will proceed with whatever
    // partial results were retrieved.
    virtual void GetCheckConnectionInfoCompleted(bool succeeded) {}

   protected:
    virtual ~Observer() {}
  };

  // Class to retrieve the external connection check results from gaia.
  // Declared publicly for unit tests.
  class ExternalCcResultFetcher : public GaiaAuthConsumer,
                                  public net::URLFetcherDelegate {
   public:
    // Maps connection URLs, as returned by StartGetCheckConnectionInfo() to
    // token and URLFetcher used to fetch the URL.
    typedef std::map<GURL, std::pair<std::string, net::URLFetcher*>>
        URLToTokenAndFetcher;

    // Maps tokens to the fetched result for that token.
    typedef std::map<std::string, std::string> ResultMap;

    ExternalCcResultFetcher(GaiaCookieManagerService* helper);
    ~ExternalCcResultFetcher() override;

    // Gets the current value of the external connection check result string.
    std::string GetExternalCcResult();

    // Start fetching the external CC result.  If a fetch is already in progress
    // it is canceled.
    void Start();

    // Are external URLs still being checked?
    bool IsRunning();

    // Returns a copy of the internal token to fetcher map.
    URLToTokenAndFetcher get_fetcher_map_for_testing() { return fetchers_; }

    // Simulate a timeout for tests.
    void TimeoutForTests();

   private:
    // Overridden from GaiaAuthConsumer.
    void OnGetCheckConnectionInfoSuccess(const std::string& data) override;
    void OnGetCheckConnectionInfoError(
        const GoogleServiceAuthError& error) override;

    // Creates and initializes a URL fetcher for doing a connection check.
    net::URLFetcher* CreateFetcher(const GURL& url);

    // Overridden from URLFetcherDelgate.
    void OnURLFetchComplete(const net::URLFetcher* source) override;

    // Any fetches still ongoing after this call are considered timed out.
    void Timeout();

    void CleanupTransientState();

    void FireGetCheckConnectionInfoCompleted(bool succeeded);

    GaiaCookieManagerService* helper_;
    base::OneShotTimer<ExternalCcResultFetcher> timer_;
    scoped_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
    URLToTokenAndFetcher fetchers_;
    ResultMap results_;
    base::Time m_external_cc_result_start_time_;

    DISALLOW_COPY_AND_ASSIGN(ExternalCcResultFetcher);
  };

  GaiaCookieManagerService(OAuth2TokenService* token_service,
                           const std::string& source,
                           SigninClient* signin_client);
  ~GaiaCookieManagerService() override;

  void AddAccountToCookie(const std::string& account_id);

  // Add or remove observers of this helper.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Cancel all login requests.
  void CancelAll();

  // Signout of |account_id| given a list of accounts already signed in.
  // Since this involves signing out of all accounts and resigning back in,
  // the order which |accounts| are given is important as it will dictate
  // the sign in order. |account_id| does not have to be in |accounts|.
  void LogOut(const std::string& account_id,
              const std::vector<std::string>& accounts);

  // Signout all accounts.
  void LogOutAllAccounts();

  // Call observers when merge session completes.  This public so that callers
  // that know that a given account is already in the cookie jar can simply
  // inform the observers.
  void SignalComplete(const std::string& account_id,
                      const GoogleServiceAuthError& error);

  // Returns true of there are pending log ins or outs.
  bool is_running() const { return accounts_.size() > 0; }

  // Start the process of fetching the external check connection result so that
  // its ready when we try to perform a merge session.
  void StartFetchingExternalCcResult();

 private:
  net::URLRequestContextGetter* request_context() {
    return signin_client_->GetURLRequestContext();
  }

  // Overridden from UbertokenConsumer.
  void OnUbertokenSuccess(const std::string& token) override;
  void OnUbertokenFailure(const GoogleServiceAuthError& error) override;

  // Overridden from GaiaAuthConsumer.
  void OnMergeSessionSuccess(const std::string& data) override;
  void OnMergeSessionFailure(const GoogleServiceAuthError& error) override;

  void LogOutInternal(const std::string& account_id,
                      const std::vector<std::string>& accounts);

  // Starts the process of fetching the uber token and then performing a merge
  // session for the next account. Virtual so that it can be overriden in tests.
  virtual void StartFetching();

  // Virtual for testing purposes.
  virtual void StartFetchingMergeSession();

  // Virtual for testing purpose.
  virtual void StartLogOutUrlFetch();

  // Start the next merge session, if needed.
  void HandleNextAccount();

  // Overridden from URLFetcherDelgate.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  OAuth2TokenService* token_service_;
  SigninClient* signin_client_;
  scoped_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  scoped_ptr<UbertokenFetcher> uber_token_fetcher_;
  ExternalCcResultFetcher external_cc_result_fetcher_;

  // If the GaiaAuthFetcher fails, retry with exponential backoff.
  net::BackoffEntry gaia_auth_fetcher_backoff_;
  base::OneShotTimer<GaiaCookieManagerService> gaia_auth_fetcher_timer_;
  int gaia_auth_fetcher_retries_;

  // The last fetched ubertoken, for use in MergeSession retries.
  std::string uber_token_;

  // A worklist for this class. Accounts names are stored here if
  // we are pending a signin action for that account. Empty strings
  // represent a signout request.
  std::deque<std::string> accounts_;

  // List of observers to notify when merge session completes.
  // Makes sure list is empty on destruction.
  ObserverList<Observer, true> observer_list_;

  // Source to use with GAIA endpoints for accounting.
  std::string source_;

  DISALLOW_COPY_AND_ASSIGN(GaiaCookieManagerService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_GAIA_COOKIE_MANAGER_SERVICE_H
