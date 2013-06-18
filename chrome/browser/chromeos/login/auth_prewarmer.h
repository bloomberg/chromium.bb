// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_PREWARMER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_PREWARMER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/net/connectivity_state_helper_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace chromeos {

// Class for prewarming authentication network connection.
class AuthPrewarmer : public ConnectivityStateHelperObserver,
                      public content::NotificationObserver {
 public:
  AuthPrewarmer();
  virtual ~AuthPrewarmer();

  void PrewarmAuthentication(const base::Closure& completion_callback);

 private:
  // chromeos::ConnectivityStateHelperObserver overrides.
  virtual void NetworkManagerChanged() OVERRIDE;
  virtual void DefaultNetworkChanged() OVERRIDE;

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  bool IsNetworkConnected() const;
  net::URLRequestContextGetter* GetRequestContext() const;
  void DoPrewarm();

  content::NotificationRegistrar registrar_;
  base::Closure completion_callback_;
  bool doing_prewarm_;

  DISALLOW_COPY_AND_ASSIGN(AuthPrewarmer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_PREWARMER_H_

