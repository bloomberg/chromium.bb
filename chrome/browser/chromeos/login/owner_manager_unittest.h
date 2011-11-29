// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_UNITTEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_UNITTEST_H_

#include "chrome/browser/chromeos/login/owner_manager.h"

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace chromeos {
class MockKeyLoadObserver : public content::NotificationObserver {
 public:
  explicit MockKeyLoadObserver(base::WaitableEvent* e);
  virtual ~MockKeyLoadObserver();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void ExpectKeyFetchSuccess(bool should_succeed);

 private:
  content::NotificationRegistrar registrar_;
  bool success_expected_;
  base::WaitableEvent* event_;
  bool observed_;
  DISALLOW_COPY_AND_ASSIGN(MockKeyLoadObserver);
};

class MockKeyUser : public OwnerManager::Delegate {
 public:
  MockKeyUser(const OwnerManager::KeyOpCode expected, base::WaitableEvent* e);
  virtual ~MockKeyUser() {}

  virtual void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                               const std::vector<uint8>& payload) OVERRIDE;

  const OwnerManager::KeyOpCode expected_;
 private:
  base::WaitableEvent* event_;
  DISALLOW_COPY_AND_ASSIGN(MockKeyUser);
};

class MockKeyUpdateUser : public OwnerManager::KeyUpdateDelegate {
 public:
  explicit MockKeyUpdateUser(base::WaitableEvent* e) : event_(e) {}
  virtual ~MockKeyUpdateUser() {}

  virtual void OnKeyUpdated() OVERRIDE;

 private:
  base::WaitableEvent* event_;
  DISALLOW_COPY_AND_ASSIGN(MockKeyUpdateUser);
};


class MockSigner : public OwnerManager::Delegate {
 public:
  MockSigner(const OwnerManager::KeyOpCode expected,
             const std::vector<uint8>& sig,
             base::WaitableEvent* e);
  virtual ~MockSigner();

  virtual void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                               const std::vector<uint8>& payload) OVERRIDE;

  const OwnerManager::KeyOpCode expected_code_;
  const std::vector<uint8> expected_sig_;

 private:
  base::WaitableEvent* event_;
  DISALLOW_COPY_AND_ASSIGN(MockSigner);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_UNITTEST_H_
