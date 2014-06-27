// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_FAKE_SIGNIN_MANAGER_H_
#define CHROME_BROWSER_SERVICES_GCM_FAKE_SIGNIN_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "components/signin/core/browser/signin_metrics.h"

#if defined(OS_CHROMEOS)
#include "components/signin/core/browser/signin_manager_base.h"
#else
#include "base/compiler_specific.h"
#include "components/signin/core/browser/signin_manager.h"
#endif

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace gcm {

#if defined(OS_CHROMEOS)
class FakeSigninManager : public SigninManagerBase {
#else
class FakeSigninManager : public SigninManager {
#endif
 public:
  explicit FakeSigninManager(Profile* profile);
  virtual ~FakeSigninManager();

  void SignIn(const std::string& username);
#if defined(OS_CHROMEOS)
  void SignOut(signin_metrics::ProfileSignout signout_source_metric);
#else
  virtual void SignOut(signin_metrics::ProfileSignout signout_source_metric)
      OVERRIDE;
#endif

  static KeyedService* Build(content::BrowserContext* context);

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FakeSigninManager);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_FAKE_SIGNIN_MANAGER_H_
