// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_STATUS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_STATUS_SERVICE_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_observer.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;
class SigninManagerBase;

namespace ntp_snippets {

enum class RemoteSuggestionsStatus : int {
  // Suggestions are enabled and the user is signed in.
  ENABLED_AND_SIGNED_IN,
  // Suggestions are enabled; the user is signed out (sign-in is not required).
  ENABLED_AND_SIGNED_OUT,
  // Suggestions have been disabled as part of the service configuration.
  EXPLICITLY_DISABLED,
  // The user is not signed in, but sign-in is required.
  SIGNED_OUT_AND_DISABLED,
};

// Aggregates data from preferences and signin to notify the provider of
// relevant changes in their states.
class RemoteSuggestionsStatusService {
 public:
  using StatusChangeCallback =
      base::Callback<void(RemoteSuggestionsStatus old_status,
                          RemoteSuggestionsStatus new_status)>;

  RemoteSuggestionsStatusService(SigninManagerBase* signin_manager,
                                 PrefService* pref_service);

  virtual ~RemoteSuggestionsStatusService();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Starts listening for changes from the dependencies. |callback| will be
  // called when a significant change in state is detected.
  void Init(const StatusChangeCallback& callback);

  // To be called when the signin state changed. Will compute the new
  // state considering the initialisation configuration and the preferences,
  // and notify via the registered callback if appropriate.
  void OnSignInStateChanged();

 private:
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsStatusServiceTest, DisabledViaPref);

  // Callback for the PrefChangeRegistrar.
  void OnSnippetsEnabledChanged();

  void OnStateChanged(RemoteSuggestionsStatus new_status);

  bool IsSignedIn() const;

  RemoteSuggestionsStatus GetStatusFromDeps() const;

  RemoteSuggestionsStatus status_;
  StatusChangeCallback status_change_callback_;

  bool require_signin_;
  SigninManagerBase* signin_manager_;
  PrefService* pref_service_;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsStatusService);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_STATUS_SERVICE_H_
