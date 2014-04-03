// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_TEST_HELPER_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_TEST_HELPER_H_

#include <string>
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/services/gcm/gcm_client_factory.h"
#include "chrome/browser/services/gcm/gcm_client_mock.h"
#include "components/signin/core/browser/signin_manager.h"

class KeyedService;
class Profile;
namespace base {
class RunLoop;
}
namespace content {
class BrowserContext;
}

namespace gcm {

// Helper class for asynchronous waiting.
class Waiter {
 public:
  Waiter();
  ~Waiter();

  // Waits until the asynchrnous operation finishes.
  void WaitUntilCompleted();

  // Signals that the asynchronous operation finishes.
  void SignalCompleted();

  // Runs until UI loop becomes idle.
  void PumpUILoop();

  // Runs until IO loop becomes idle.
  void PumpIOLoop();

 private:
  void PumpIOLoopCompleted();
  void OnIOLoopPump();
  void OnIOLoopPumpCompleted();

  scoped_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(Waiter);
};

#if defined(OS_CHROMEOS)
class FakeSigninManager : public SigninManagerBase {
#else
class FakeSigninManager : public SigninManager {
#endif
 public:
  static KeyedService* Build(content::BrowserContext* context);

  explicit FakeSigninManager(Profile* profile);
  virtual ~FakeSigninManager();

  void SignIn(const std::string& username);
#if defined(OS_CHROMEOS)
  void SignOut();
#else
  virtual void SignOut() OVERRIDE;
#endif

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FakeSigninManager);
};

class FakeGCMClientFactory : public GCMClientFactory {
 public:
  explicit FakeGCMClientFactory(
      GCMClientMock::LoadingDelay gcm_client_loading_delay);
  virtual ~FakeGCMClientFactory();

  virtual scoped_ptr<GCMClient> BuildInstance() OVERRIDE;

 private:
  GCMClientMock::LoadingDelay gcm_client_loading_delay_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMClientFactory);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_TEST_HELPER_H_
