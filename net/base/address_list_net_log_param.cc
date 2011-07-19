// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list_net_log_param.h"

#include "base/values.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"

namespace net {

AddressListNetLogParam::AddressListNetLogParam(const AddressList& address_list)
    : address_list_(address_list) {
}

Value* AddressListNetLogParam::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  ListValue* list = new ListValue();

  for (const addrinfo* head = address_list_.head();
       head != NULL ; head = head->ai_next) {
    list->Append(Value::CreateStringValue(NetAddressToStringWithPort(head)));
  }

  dict->Set("address_list", list);
  return dict;
}

}  // namespace
