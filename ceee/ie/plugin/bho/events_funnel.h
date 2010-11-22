// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Common base class for funnels of Chrome Extension events that originate
// from the BHO.

#ifndef CEEE_IE_PLUGIN_BHO_EVENTS_FUNNEL_H_
#define CEEE_IE_PLUGIN_BHO_EVENTS_FUNNEL_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

class Value;

class BrokerRpcClient;

// Defines a base class for sending events to the Broker.
class EventsFunnel {
 protected:
  EventsFunnel();
  virtual ~EventsFunnel();

  // Send the given event to the Broker.
  // @param event_name The name of the event.
  // @param event_args The arguments to be sent with the event.
  // protected virtual for testability...
  virtual HRESULT SendEvent(const char* event_name, const Value& event_args);

 protected:
  virtual HRESULT SendEventToBroker(const char* event_name,
                                    const char* event_args);

 private:
  // Pointer to broker client. If is not NULL attempt to connect was performed
  // and new attempts are not nessesary.
  scoped_ptr<BrokerRpcClient> broker_rpc_client_;
  DISALLOW_COPY_AND_ASSIGN(EventsFunnel);
};

#endif  // CEEE_IE_PLUGIN_BHO_EVENTS_FUNNEL_H_
