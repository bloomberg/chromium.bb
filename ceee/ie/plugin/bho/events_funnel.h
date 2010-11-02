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

class Value;

// Defines a base class for sending events to the Broker.
class EventsFunnel {
 protected:
  // @param keep_broker_alive If true broker will be alive during
  // lifetime of this funnel, otherwise only during SendEvent.
  explicit EventsFunnel(bool keep_broker_alive);
  virtual ~EventsFunnel();

  // Send the given event to the Broker.
  // @param event_name The name of the event.
  // @param event_args The arguments to be sent with the event.
  // protected virtual for testability...
  virtual HRESULT SendEvent(const char* event_name, const Value& event_args);

 private:
  // If true constructor/destructor of class increments/decrements ref counter
  // of broker thread. Otherwise SendEvent does it for every event.
  const bool keep_broker_alive_;
  DISALLOW_COPY_AND_ASSIGN(EventsFunnel);
};

#endif  // CEEE_IE_PLUGIN_BHO_EVENTS_FUNNEL_H_
