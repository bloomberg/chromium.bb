// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_SIGNIN_STATUS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_SIGNIN_STATUS_METRICS_PROVIDER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/metrics/metrics_provider.h"
#include "components/signin/core/browser/signin_manager_base.h"

class Browser;

namespace base {
class FilePath;
}

// Collect login status of all opened profiles during one UMA session and record
// the value into a histogram before UMA log is uploaded. It's currently not
// supported on platform chromeos, Android or iOS.
class SigninStatusMetricsProvider : public metrics::MetricsProvider,
                                    public chrome::BrowserListObserver,
                                    public SigninManagerBase::Observer,
                                    public SigninManagerFactory::Observer {
 public:
  virtual ~SigninStatusMetricsProvider();

  // Record the collected sign-in status into a histogram and re-check current
  // sign-in status to get prepared for the next UMA session. Called by
  // MetricsServiceClient when it is collecting final metrics.
  void RecordSigninStatusHistogram();

  // Factory method, creates a new instance of this class.
  static SigninStatusMetricsProvider* CreateInstance();

 private:
  // The boolean |is_test| indicates whether or not this is an instance for
  // testing purpose. If so, skip the initialization. Except for testing
  // purpose, this class's instance should be created through the static
  // CreateInstance() method.
  explicit SigninStatusMetricsProvider(bool is_test);

  FRIEND_TEST_ALL_PREFIXES(SigninStatusMetricsProvider,
                           UpdateInitialSigninStatus);
  FRIEND_TEST_ALL_PREFIXES(SigninStatusMetricsProvider,
                           UpdateStatusWhenBrowserAdded);
  FRIEND_TEST_ALL_PREFIXES(SigninStatusMetricsProvider, GoogleSigninSucceeded);
  FRIEND_TEST_ALL_PREFIXES(SigninStatusMetricsProvider, GoogleSignedOut);

  // Possible sign-in status of all opened profiles during one UMA session. For
  // MIXED_SIGNIN_STATUS, at least one signed-in profile and at least one
  // unsigned-in profile were opened between two UMA log uploads.
  enum ProfilesSigninStatus {
    ALL_PROFILES_SIGNED_IN,
    ALL_PROFILES_NOT_SIGNED_IN,
    MIXED_SIGNIN_STATUS,
    SIGNIN_STATUS_MAX,
  };

  // chrome::BrowserListObserver:
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE;

  // SigninManagerFactory::Observer:
  virtual void SigninManagerCreated(SigninManagerBase* manager) OVERRIDE;
  virtual void SigninManagerShutdown(SigninManagerBase* manager) OVERRIDE;

  // SigninManagerBase::Observer:
  virtual void GoogleSigninSucceeded(const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void GoogleSignedOut(const std::string& username) OVERRIDE;

  // Obtain sign-in status and add observers.
  void Initialize();

  // Update the sign-in status based on all currently opened profiles. Called by
  // ComputeCurrentSigninStatus at class construction and right after each UMA
  // log upload. |total_count| is the number of opened profiles and
  // |signed_in_count| represents the number of signed-in profiles among those
  // |total_count| profiles.
  void UpdateInitialSigninStatus(size_t total_count, size_t signed_in_count);

  // Update the sign-in status right after a new browser is opened.
  void UpdateStatusWhenBrowserAdded(bool signed_in);

  // Compute current sign-in status of all opened profiles.
  void ComputeCurrentSigninStatus();

  // Get the current recorded sign-in status. For testing purpose only.
  ProfilesSigninStatus GetSigninStatusForTesting();

  // Sign-in status of all profiles seen so far.
  ProfilesSigninStatus signin_status_;

  // Used to track the SigninManagers that this instance is observing so that
  // this instance can be removed as an observer on its destruction.
  ScopedObserver<SigninManagerBase, SigninStatusMetricsProvider>
      scoped_observer_;

  // Whether the instance is for testing or not.
  bool is_test_;

  base::WeakPtrFactory<SigninStatusMetricsProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninStatusMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_SIGNIN_STATUS_METRICS_PROVIDER_H_
