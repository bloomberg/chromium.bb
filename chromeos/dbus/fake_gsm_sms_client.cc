// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/fake_gsm_sms_client.h"

namespace chromeos {

FakeGsmSMSClient::FakeGsmSMSClient()
    : test_index_(-1),
      sms_test_message_switch_present_(false),
      weak_ptr_factory_(this) {
  test_messages_.push_back("Test Message 0");
  test_messages_.push_back("Test Message 1");
  test_messages_.push_back("Test a relatively long message 2");
  test_messages_.push_back("Test a very, the quick brown fox jumped"
                           " over the lazy dog, long message 3");
  test_messages_.push_back("Test Message 4");
  test_messages_.push_back("Test Message 5");
  test_messages_.push_back("Test Message 6");
}

FakeGsmSMSClient::~FakeGsmSMSClient() {
}

void FakeGsmSMSClient::Init(dbus::Bus* bus) {
}

void FakeGsmSMSClient::SetSmsReceivedHandler(
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    const SmsReceivedHandler& handler) {
  handler_ = handler;
}

void FakeGsmSMSClient::ResetSmsReceivedHandler(
    const std::string& service_name,
    const dbus::ObjectPath& object_path) {
  handler_.Reset();
}

void FakeGsmSMSClient::Delete(const std::string& service_name,
                              const dbus::ObjectPath& object_path,
                              uint32_t index,
                              const DeleteCallback& callback) {
  message_list_.Remove(index, NULL);
  callback.Run();
}

void FakeGsmSMSClient::Get(const std::string& service_name,
                           const dbus::ObjectPath& object_path,
                           uint32_t index,
                           const GetCallback& callback) {
  base::DictionaryValue* dictionary = NULL;
  if (message_list_.GetDictionary(index, &dictionary)) {
    callback.Run(*dictionary);
    return;
  }
  base::DictionaryValue empty_dictionary;
  callback.Run(empty_dictionary);
}

void FakeGsmSMSClient::List(const std::string& service_name,
                            const dbus::ObjectPath& object_path,
                            const ListCallback& callback) {
  callback.Run(message_list_);
}

void FakeGsmSMSClient::RequestUpdate(const std::string& service_name,
                                     const dbus::ObjectPath& object_path) {
  if (!sms_test_message_switch_present_)
    return;

  if (test_index_ >= 0)
    return;
  test_index_ = 0;
  // Call PushTestMessageChain asynchronously so that the handler_ callback
  // does not get called from the update request.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FakeGsmSMSClient::PushTestMessageChain,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FakeGsmSMSClient::PushTestMessageChain() {
  if (PushTestMessage())
    PushTestMessageDelayed();
}

void FakeGsmSMSClient::PushTestMessageDelayed() {
  const int kSmsMessageDelaySeconds = 5;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&FakeGsmSMSClient::PushTestMessageChain,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kSmsMessageDelaySeconds));
}

bool FakeGsmSMSClient::PushTestMessage() {
  if (test_index_ >= static_cast<int>(test_messages_.size()))
    return false;
  auto message = base::MakeUnique<base::DictionaryValue>();
  message->SetString("number", "000-000-0000");
  message->SetString("text", test_messages_[test_index_]);
  message->SetInteger("index", test_index_);
  int msg_index = message_list_.GetSize();
  message_list_.Append(std::move(message));
  if (!handler_.is_null())
    handler_.Run(msg_index, true);
  ++test_index_;
  return true;
}

}  // namespace chromeos
