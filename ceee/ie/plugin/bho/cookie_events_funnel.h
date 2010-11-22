// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Cookie Events.

#ifndef CEEE_IE_PLUGIN_BHO_COOKIE_EVENTS_FUNNEL_H_
#define CEEE_IE_PLUGIN_BHO_COOKIE_EVENTS_FUNNEL_H_

#include "ceee/ie/broker/cookie_api_module.h"
#include "ceee/ie/plugin/bho/events_funnel.h"

// Implements a set of methods to send cookie related events to the Broker.
class CookieEventsFunnel : public EventsFunnel {
 public:
  CookieEventsFunnel() {}

  // Sends the cookies.onChanged event to the Broker.
  // @param removed True if the cookie was removed vs. set.
  // @param cookie Information about the cookie that was set or removed.
  virtual HRESULT OnChanged(bool removed,
                            const cookie_api::CookieInfo& cookie);

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieEventsFunnel);
};

#endif  // CEEE_IE_PLUGIN_BHO_COOKIE_EVENTS_FUNNEL_H_
