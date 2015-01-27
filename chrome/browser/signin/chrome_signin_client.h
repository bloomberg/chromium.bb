// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "content/public/browser/render_process_host_observer.h"

class CookieSettings;
class Profile;

class ChromeSigninClient : public SigninClient,
                           public content::RenderProcessHostObserver,
                           public SigninErrorController::Observer {
 public:
  explicit ChromeSigninClient(
      Profile* profile, SigninErrorController* signin_error_controller);
  ~ChromeSigninClient() override;

  // Utility methods.
  static bool ProfileAllowsSigninCookies(Profile* profile);
  static bool SettingsAllowSigninCookies(CookieSettings* cookie_settings);

  // Tracks the privileged signin process identified by |host_id| so that we
  // can later ask (via IsSigninProcess) if it is safe to sign the user in from
  // the current context (see OneClickSigninHelper).  All of this tracking
  // state is reset once the renderer process terminates.
  //
  // N.B. This is the id returned by RenderProcessHost::GetID().
  // TODO(guohui): Eliminate these APIs once the web-based signin flow is
  // replaced by a native flow. crbug.com/347247
  void SetSigninProcess(int host_id) override;
  void ClearSigninProcess() override;
  bool IsSigninProcess(int host_id) const override;
  bool HasSigninProcess() const override;

  // content::RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // SigninClient implementation.
  PrefService* GetPrefs() override;
  scoped_refptr<TokenWebData> GetDatabase() override;
  bool CanRevokeCredentials() override;
  std::string GetSigninScopedDeviceId() override;
  void OnSignedOut() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  bool ShouldMergeSigninCredentialsIntoCookieJar() override;
  bool IsFirstRun() const override;
  base::Time GetInstallDate() override;
  bool AreSigninCookiesAllowed() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;

  // Returns a string describing the chrome version environment. Version format:
  // <Build Info> <OS> <Version number> (<Last change>)<channel or "-devel">
  // If version information is unavailable, returns "invalid."
  std::string GetProductVersion() override;
  scoped_ptr<CookieChangedSubscription> AddCookieChangedCallback(
      const GURL& url,
      const std::string& name,
      const net::CookieStore::CookieChangedCallback& callback) override;
  void OnSignedIn(const std::string& account_id,
                  const std::string& username,
                  const std::string& password) override;
  void PostSignedIn(const std::string& account_id,
                    const std::string& username,
                    const std::string& password) override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

 private:
  Profile* profile_;

  SigninErrorController* signin_error_controller_;

  // See SetSigninProcess. Tracks the currently active signin process
  // by ID, if there is one.
  int signin_host_id_;

  // The RenderProcessHosts being observed.
  std::set<content::RenderProcessHost*> signin_hosts_observed_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSigninClient);
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
