// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/fake_sms_client.h"
#include "dbus/object_path.h"

namespace chromeos {

FakeSMSClient::FakeSMSClient() : weak_ptr_factory_(this) {}

FakeSMSClient::~FakeSMSClient() {}

void FakeSMSClient::Init(dbus::Bus* bus) {}

void FakeSMSClient::GetAll(const std::string& service_name,
                           const dbus::ObjectPath& object_path,
                           const GetAllCallback& callback) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
           chromeos::switches::kSmsTestMessages))
    return;

  // Ownership passed to callback
  base::DictionaryValue* sms = new base::DictionaryValue();
  sms->SetString("Number", "000-000-0000");
  sms->SetString("Text", "FakeSMSClient: Test Message: " + object_path.value());
  sms->SetString("Timestamp", "Fri Jun  8 13:26:04 EDT 2012");

  // Run callback asynchronously.
  if (callback.is_null())
    return;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeSMSClient::OnGetAll,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(sms),
                 callback));
}

void FakeSMSClient::OnGetAll(base::DictionaryValue* sms,
                             const GetAllCallback& callback) {
  callback.Run(*sms);
}

}  // namespace chromeos
