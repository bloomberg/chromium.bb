// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_MODEM_MESSAGING_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_MODEM_MESSAGING_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/modem_messaging_client.h"

namespace chromeos {

class CHROMEOS_EXPORT FakeModemMessagingClient : public ModemMessagingClient {
 public:
  FakeModemMessagingClient();
  virtual ~FakeModemMessagingClient();

  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void SetSmsReceivedHandler(const std::string& service_name,
                                     const dbus::ObjectPath& object_path,
                                     const SmsReceivedHandler& handler)
      OVERRIDE;
  virtual void ResetSmsReceivedHandler(const std::string& service_name,
                                       const dbus::ObjectPath& object_path)
      OVERRIDE;
  virtual void Delete(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      const dbus::ObjectPath& sms_path,
                      const DeleteCallback& callback) OVERRIDE;
  virtual void List(const std::string& service_name,
                    const dbus::ObjectPath& object_path,
                    const ListCallback& callback) OVERRIDE;

 private:
  SmsReceivedHandler sms_received_handler_;
  std::vector<dbus::ObjectPath> message_paths_;

  DISALLOW_COPY_AND_ASSIGN(FakeModemMessagingClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_MODEM_MESSAGING_CLIENT_H_
