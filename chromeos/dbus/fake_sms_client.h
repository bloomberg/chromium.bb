// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SMS_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SMS_CLIENT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/sms_client.h"

namespace chromeos {

class FakeSMSClient : public SMSClient {
 public:
  FakeSMSClient();
  ~FakeSMSClient() override;

  void Init(dbus::Bus* bus) override;

  void GetAll(const std::string& service_name,
              const dbus::ObjectPath& object_path,
              const GetAllCallback& callback) override;

 private:
  void OnGetAll(base::DictionaryValue* sms, const GetAllCallback& callback);

  base::WeakPtrFactory<FakeSMSClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeSMSClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SMS_CLIENT_H_
