// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Common base class for funnels of Chrome Extension events that originate
// from the BHO and are sent to the Broker.

#include "ceee/ie/plugin/bho/events_funnel.h"

#include <atlbase.h>
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "ceee/ie/broker/broker_rpc_client.h"
#include "ceee/ie/common/ceee_module_util.h"


EventsFunnel::EventsFunnel() {
}

EventsFunnel::~EventsFunnel() {
}

HRESULT EventsFunnel::SendEvent(const char* event_name,
                                const Value& event_args) {
  // Event arguments for FireEventToBroker always need to be stored in a list.
  std::string event_args_str;
  if (event_args.IsType(Value::TYPE_LIST)) {
    base::JSONWriter::Write(&event_args, false, &event_args_str);
  } else {
    ListValue list;
    list.Append(event_args.DeepCopy());
    base::JSONWriter::Write(&list, false, &event_args_str);
  }

  return SendEventToBroker(event_name, event_args_str.c_str());
}

HRESULT EventsFunnel::SendEventToBroker(const char* event_name,
                                        const char* event_args) {
  if (!broker_rpc_client_.get()) {
    broker_rpc_client_.reset(new BrokerRpcClient());
    if (!broker_rpc_client_.get())
      return E_OUTOFMEMORY;
    HRESULT hr = broker_rpc_client_->Connect(true);
    // Don't reset broker_rpc_client_. See comment in *h file.
    if (FAILED(hr))
      return hr;
  }
  return broker_rpc_client_->FireEvent(event_name, event_args);
}
