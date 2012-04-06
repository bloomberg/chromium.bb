// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session_manager_observer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "chrome/browser/chromeos/login/signed_settings_cache.h"
#include "chrome/common/chrome_notification_types.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

namespace {

class StubDelegate
    : public SignedSettings::Delegate<
  const enterprise_management::PolicyFetchResponse&> {
 public:
  StubDelegate() : policy_fetcher_(NULL) {}
  virtual ~StubDelegate() {}

  void set_fetcher(SignedSettings* fetcher) { policy_fetcher_ = fetcher; }
  SignedSettings* fetcher() { return policy_fetcher_.get(); }

  // Implementation of SignedSettings::Delegate
  virtual void OnSettingsOpCompleted(
      SignedSettings::ReturnCode code,
      const enterprise_management::PolicyFetchResponse& value) {
    VLOG(1) << "Done Fetching Policy";
    delete this;
  }

 private:
  scoped_refptr<SignedSettings> policy_fetcher_;
  DISALLOW_COPY_AND_ASSIGN(StubDelegate);
};

}  // namespace

SessionManagerObserver::SessionManagerObserver() {
  DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);
}

SessionManagerObserver::~SessionManagerObserver() {
  DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
}

void SessionManagerObserver::OwnerKeySet(bool success) {
  VLOG(1) << "Owner key generation: " << (success ? "success" : "fail");
  int result =
      chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED;
  if (!success)
    result = chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_FAILED;

  // We stored some settings in transient storage before owner was assigned.
  // Now owner is assigned and key is generated and we should persist
  // those settings into signed storage.
  if (success && g_browser_process && g_browser_process->local_state())
    signed_settings_cache::Finalize(g_browser_process->local_state());

  // Whether we exported the public key or not, send a notification
  // indicating that we're done with this attempt.
  content::NotificationService::current()->Notify(
      result,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void SessionManagerObserver::PropertyChangeComplete(bool success) {
  if (success) {
    StubDelegate* stub = new StubDelegate();  // Manages its own lifetime.
    stub->set_fetcher(SignedSettings::CreateRetrievePolicyOp(stub));
    stub->fetcher()->Execute();
  }
}

}  // namespace chromeos
