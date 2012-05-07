// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list_net_log_param.h"

#include "base/values.h"
#include "net/base/net_util.h"

namespace net {

AddressListNetLogParam::AddressListNetLogParam(const AddressList& address_list)
    : address_list_(address_list) {
}

Value* AddressListNetLogParam::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  ListValue* list = new ListValue();

  for (size_t i = 0; i < address_list_.size() ; ++i) {
    list->Append(Value::CreateStringValue(address_list_[i].ToString()));
  }

  dict->Set("address_list", list);
  return dict;
}

}  // namespace
