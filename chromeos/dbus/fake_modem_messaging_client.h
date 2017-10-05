// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_MODEM_MESSAGING_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_MODEM_MESSAGING_CLIENT_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/modem_messaging_client.h"

namespace chromeos {

class CHROMEOS_EXPORT FakeModemMessagingClient : public ModemMessagingClient {
 public:
  FakeModemMessagingClient();
  ~FakeModemMessagingClient() override;

  void Init(dbus::Bus* bus) override;
  void SetSmsReceivedHandler(const std::string& service_name,
                             const dbus::ObjectPath& object_path,
                             const SmsReceivedHandler& handler) override;
  void ResetSmsReceivedHandler(const std::string& service_name,
                               const dbus::ObjectPath& object_path) override;
  void Delete(const std::string& service_name,
              const dbus::ObjectPath& object_path,
              const dbus::ObjectPath& sms_path,
              const DeleteCallback& callback) override;
  void List(const std::string& service_name,
            const dbus::ObjectPath& object_path,
            const ListCallback& callback) override;

 private:
  SmsReceivedHandler sms_received_handler_;
  std::vector<dbus::ObjectPath> message_paths_;

  DISALLOW_COPY_AND_ASSIGN(FakeModemMessagingClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_MODEM_MESSAGING_CLIENT_H_
