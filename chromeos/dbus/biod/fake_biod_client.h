// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BIOD_FAKE_BIOD_CLIENT_H_
#define CHROMEOS_DBUS_BIOD_FAKE_BIOD_CLIENT_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/biod/biod_client.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

// A fake implementation of BiodClient.
class CHROMEOS_EXPORT FakeBiodClient : public BiodClient {
 public:
  FakeBiodClient();
  ~FakeBiodClient() override;

  // Emulates the biod daemon by sending events which the daemon normally sends.
  // Notifies |observers_| about various events. These will be used in tests.
  void SendEnrollScanDone(biod::ScanResult type_result, bool is_complete);
  void SendAuthScanDone(biod::ScanResult type_result,
                        const AuthScanMatches& matches);
  void SendSessionFailed();

  // BiodClient:
  void Init(dbus::Bus* bus) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  void StartEnrollSession(const std::string& user_id,
                          const std::string& label,
                          const ObjectPathCallback& callback) override;
  void GetRecordsForUser(const std::string& user_id,
                         const UserRecordsCallback& callback) override;
  void DestroyAllRecords(const VoidDBusMethodCallback& callback) override;
  void StartAuthSession(const ObjectPathCallback& callback) override;
  void RequestType(const BiometricTypeCallback& callback) override;
  void CancelEnrollSession(const dbus::ObjectPath& enroll_session_path,
                           const VoidDBusMethodCallback& callback) override;
  void EndAuthSession(const dbus::ObjectPath& auth_session_path,
                      const VoidDBusMethodCallback& callback) override;
  void SetRecordLabel(const dbus::ObjectPath& record_path,
                      const std::string& label,
                      const VoidDBusMethodCallback& callback) override;
  void RemoveRecord(const dbus::ObjectPath& record_path,
                    const VoidDBusMethodCallback& callback) override;
  void RequestRecordLabel(const dbus::ObjectPath& record_path,
                          const LabelCallback& callback) override;

 private:
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeBiodClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BIOD_FAKE_BIOD_CLIENT_H_
