// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_TEST_SIGNIN_CLIENT_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_TEST_SIGNIN_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/signin/core/browser/signin_client.h"
#include "net/url_request/url_request_test_util.h"

#if defined(OS_IOS)
#include "ios/public/test/fake_profile_oauth2_token_service_ios_provider.h"
#endif

class PrefService;

// An implementation of SigninClient for use in unittests. Instantiates test
// versions of the various objects that SigninClient is required to provide as
// part of its interface.
class TestSigninClient : public SigninClient {
 public:
  TestSigninClient();
  TestSigninClient(PrefService* pref_service);
  virtual ~TestSigninClient();

  // SigninClient implementation that is specialized for unit tests.

  // Returns NULL.
  // NOTE: This should be changed to return a properly-initalized PrefService
  // once there is a unit test that requires it.
  virtual PrefService* GetPrefs() OVERRIDE;

  // Returns a pointer to a loaded database.
  virtual scoped_refptr<TokenWebData> GetDatabase() OVERRIDE;

  // Returns true.
  virtual bool CanRevokeCredentials() OVERRIDE;

  // Returns empty string.
  virtual std::string GetSigninScopedDeviceId() OVERRIDE;

  // Does nothing.
  virtual void ClearSigninScopedDeviceId() OVERRIDE;

  // Returns the empty string.
  virtual std::string GetProductVersion() OVERRIDE;

  // Returns a TestURLRequestContextGetter.
  virtual net::URLRequestContextGetter* GetURLRequestContext() OVERRIDE;

#if defined(OS_IOS)
  virtual ios::ProfileOAuth2TokenServiceIOSProvider* GetIOSProvider() OVERRIDE;
#endif

  // Returns true.
  virtual bool ShouldMergeSigninCredentialsIntoCookieJar() OVERRIDE;

  // Does nothing.
  virtual scoped_ptr<CookieChangedCallbackList::Subscription>
      AddCookieChangedCallback(const CookieChangedCallback& callback) OVERRIDE;

#if defined(OS_IOS)
  ios::FakeProfileOAuth2TokenServiceIOSProvider* GetIOSProviderAsFake();
#endif

  // SigninClient overrides:
  virtual void SetSigninProcess(int host_id) OVERRIDE;
  virtual void ClearSigninProcess() OVERRIDE;
  virtual bool IsSigninProcess(int host_id) const OVERRIDE;
  virtual bool HasSigninProcess() const OVERRIDE;

 private:
  // Loads the token database.
  void LoadDatabase();

  base::ScopedTempDir temp_dir_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  scoped_refptr<TokenWebData> database_;
  int signin_host_id_;
  CookieChangedCallbackList cookie_callbacks_;

  PrefService* pref_service_;

#if defined(OS_IOS)
  scoped_ptr<ios::FakeProfileOAuth2TokenServiceIOSProvider> iosProvider_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestSigninClient);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_TEST_SIGNIN_CLIENT_H_
