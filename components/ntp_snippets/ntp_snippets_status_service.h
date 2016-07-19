// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_STATUS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_STATUS_SERVICE_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_driver/sync_service_observer.h"

class PrefRegistrySimple;
class PrefService;
class SigninManagerBase;

namespace sync_driver {
class SyncService;
}

namespace ntp_snippets {

// On Android builds, a Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp.snippets
enum class DisabledReason : int {
  // Snippets are enabled
  NONE,
  // Snippets have been disabled as part of the service configuration.
  EXPLICITLY_DISABLED,
  // The user is not signed in, and the service requires it to be enabled.
  SIGNED_OUT,
  // Sync is not enabled, and the service requires it to be enabled.
  SYNC_DISABLED,
  // The service requires passphrase encryption to be disabled.
  PASSPHRASE_ENCRYPTION_ENABLED,
  // History sync is not enabled, and the service requires it to be enabled.
  HISTORY_SYNC_DISABLED,
  // The sync service is not completely initialized, and the status is unknown.
  HISTORY_SYNC_STATE_UNKNOWN
};

// Aggregates data from sync and signin to notify the snippet service of
// relevant changes in their states.
class NTPSnippetsStatusService : public sync_driver::SyncServiceObserver {
 public:
  typedef base::Callback<void(DisabledReason)> DisabledReasonChangeCallback;

  NTPSnippetsStatusService(SigninManagerBase* signin_manager,
                           sync_driver::SyncService* sync_service,
                           PrefService* pref_service);

  ~NTPSnippetsStatusService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Starts listening for changes from the dependencies. |callback| will be
  // called when a significant change in state is detected.
  void Init(const DisabledReasonChangeCallback& callback);

  DisabledReason disabled_reason() const { return disabled_reason_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsStatusServiceTest,
                           SyncStateCompatibility);
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsStatusServiceTest, DisabledViaPref);

  // sync_driver::SyncServiceObserver implementation. Also used as a callback
  // for the PrefChangeRegistrar.
  void OnStateChanged() override;

  DisabledReason GetDisabledReasonFromDeps() const;

  DisabledReason disabled_reason_;
  DisabledReasonChangeCallback disabled_reason_change_callback_;

  SigninManagerBase* signin_manager_;
  sync_driver::SyncService* sync_service_;
  PrefService* pref_service_;

  PrefChangeRegistrar pref_change_registrar_;

  // The observer for the SyncService. When the sync state changes,
  // SyncService will call |OnStateChanged|.
  ScopedObserver<sync_driver::SyncService, sync_driver::SyncServiceObserver>
      sync_service_observer_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsStatusService);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_STATUS_SERVICE_H_
