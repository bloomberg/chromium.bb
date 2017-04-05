// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/biod/fake_biod_client.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

FakeBiodClient::FakeBiodClient() {}

FakeBiodClient::~FakeBiodClient() {}

void FakeBiodClient::SendEnrollScanDone(biod::ScanResult type_result,
                                        bool is_complete) {
  for (auto& observer : observers_)
    observer.BiodEnrollScanDoneReceived(type_result, is_complete);
}

void FakeBiodClient::SendAuthScanDone(biod::ScanResult type_result,
                                      const AuthScanMatches& matches) {
  for (auto& observer : observers_)
    observer.BiodAuthScanDoneReceived(type_result, matches);
}

void FakeBiodClient::SendSessionFailed() {
  for (auto& observer : observers_)
    observer.BiodSessionFailedReceived();
}

void FakeBiodClient::Init(dbus::Bus* bus) {}

void FakeBiodClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBiodClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeBiodClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void FakeBiodClient::StartEnrollSession(const std::string& /* user_id */,
                                        const std::string& /* label */,
                                        const ObjectPathCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, dbus::ObjectPath()));
}

void FakeBiodClient::GetRecordsForUser(const std::string& /* user_id */,
                                       const UserRecordsCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, std::vector<dbus::ObjectPath>()));
}

void FakeBiodClient::DestroyAllRecords() {}

void FakeBiodClient::StartAuthSession(const ObjectPathCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, dbus::ObjectPath()));
}

void FakeBiodClient::RequestType(const BiometricTypeCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, biod::BiometricType::BIOMETRIC_TYPE_UNKNOWN));
}

void FakeBiodClient::CancelEnrollSession(
    const dbus::ObjectPath& /* enroll_session_path */) {}

void FakeBiodClient::EndAuthSession(
    const dbus::ObjectPath& /* auth_session_path */) {}

void FakeBiodClient::SetRecordLabel(const dbus::ObjectPath& /* record_path */,
                                    const std::string& /* label */) {}

void FakeBiodClient::RemoveRecord(const dbus::ObjectPath& /* record_path */) {}

void FakeBiodClient::RequestRecordLabel(
    const dbus::ObjectPath& /* record_path */,
    const LabelCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, ""));
}

}  // namespace chromeos
