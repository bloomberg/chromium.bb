// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_NFC_RECORD_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_NFC_RECORD_CLIENT_H_

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/nfc_record_client.h"
#include "dbus/object_path.h"

namespace chromeos {

// FakeNfcRecordClient simulates the behavior of the NFC record objects and is
// used both in test cases in place of a mock and on the Linux desktop.
// TODO(armansito): For now, this doesn't do anything. Implement fake
// behavior in conjunction with unit tests while implementing the src/device
// layer.
class CHROMEOS_EXPORT FakeNfcRecordClient : public NfcRecordClient {
 public:
  struct Properties : public NfcRecordClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    virtual ~Properties();

    // dbus::PropertySet overrides.
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeNfcRecordClient();
  virtual ~FakeNfcRecordClient();

  // NfcTagClient overrides.
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual Properties* GetProperties(
      const dbus::ObjectPath& object_path) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeNfcRecordClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_NFC_RECORD_CLIENT_H_
