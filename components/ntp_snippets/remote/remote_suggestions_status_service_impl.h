// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_STATUS_SERVICE_IMPL_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_STATUS_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_observer.h"
#include "components/ntp_snippets/remote/remote_suggestions_status_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace ntp_snippets {

class RemoteSuggestionsStatusServiceImpl
    : public RemoteSuggestionsStatusService {
 public:
  RemoteSuggestionsStatusServiceImpl(bool is_signed_in,
                                     PrefService* pref_service,
                                     const std::string& additional_toggle_pref);

  ~RemoteSuggestionsStatusServiceImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // RemoteSuggestionsStatusService implementation.
  void Init(const StatusChangeCallback& callback) override;
  void OnSignInStateChanged(bool has_signed_in) override;

 private:
  // TODO(jkrcal): Rewrite the tests using the public API - observing status
  // changes instead of calling private GetStatusFromDeps() directly.
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsStatusServiceImplTest,
                           SigninNeededIfSpecifiedByParam);
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsStatusServiceImplTest,
                           NoSigninNeeded);
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsStatusServiceImplTest,
                           DisabledViaPref);

  // Callback for the PrefChangeRegistrar.
  void OnSnippetsEnabledChanged();

  void OnStateChanged(RemoteSuggestionsStatus new_status);

  bool IsSignedIn() const;

  // Returns whether the service is explicitly disabled, by the user or by a
  // policy for example.
  bool IsExplicitlyDisabled() const;

  RemoteSuggestionsStatus GetStatusFromDeps() const;

  RemoteSuggestionsStatus status_;
  StatusChangeCallback status_change_callback_;

  // Name of a preference to be used as an additional toggle to guard the
  // remote suggestions provider.
  std::string additional_toggle_pref_;

  bool is_signed_in_;
  PrefService* pref_service_;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsStatusServiceImpl);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_STATUS_SERVICE_IMPL_H_
