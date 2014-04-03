// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service_test_helper.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

namespace gcm {

Waiter::Waiter() {
}

Waiter::~Waiter() {
}

void Waiter::WaitUntilCompleted() {
  run_loop_.reset(new base::RunLoop);
  run_loop_->Run();
}

void Waiter::SignalCompleted() {
  if (run_loop_ && run_loop_->running())
    run_loop_->Quit();
}

void Waiter::PumpUILoop() {
  base::MessageLoop::current()->RunUntilIdle();
}

void Waiter::PumpIOLoop() {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&Waiter::OnIOLoopPump, base::Unretained(this)));

  WaitUntilCompleted();
}

void Waiter::PumpIOLoopCompleted() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  SignalCompleted();
}

void Waiter::OnIOLoopPump() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&Waiter::OnIOLoopPumpCompleted, base::Unretained(this)));
}

void Waiter::OnIOLoopPumpCompleted() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&Waiter::PumpIOLoopCompleted,
                 base::Unretained(this)));
}

// static
KeyedService* FakeSigninManager::Build(content::BrowserContext* context) {
  return new FakeSigninManager(static_cast<Profile*>(context));
}

FakeSigninManager::FakeSigninManager(Profile* profile)
#if defined(OS_CHROMEOS)
    : SigninManagerBase(
          ChromeSigninClientFactory::GetInstance()->GetForProfile(profile)),
#else
    : SigninManager(
          ChromeSigninClientFactory::GetInstance()->GetForProfile(profile),
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile)),
#endif
      profile_(profile) {
  Initialize(NULL);
}

FakeSigninManager::~FakeSigninManager() {
}

void FakeSigninManager::SignIn(const std::string& username) {
  SetAuthenticatedUsername(username);
  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    GoogleSigninSucceeded(username, std::string()));
}

void FakeSigninManager::SignOut() {
  std::string username = GetAuthenticatedUsername();
  clear_authenticated_username();
  profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);
  FOR_EACH_OBSERVER(Observer, observer_list_, GoogleSignedOut(username));
}

FakeGCMClientFactory::FakeGCMClientFactory(
    GCMClientMock::LoadingDelay gcm_client_loading_delay)
    : gcm_client_loading_delay_(gcm_client_loading_delay) {
}

FakeGCMClientFactory::~FakeGCMClientFactory() {
}

scoped_ptr<GCMClient> FakeGCMClientFactory::BuildInstance() {
  return scoped_ptr<GCMClient>(new GCMClientMock(gcm_client_loading_delay_));
}

}  // namespace gcm
