// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Infobar Events.

#ifndef CEEE_IE_PLUGIN_BHO_INFOBAR_EVENTS_FUNNEL_H_
#define CEEE_IE_PLUGIN_BHO_INFOBAR_EVENTS_FUNNEL_H_

#include "ceee/ie/plugin/bho/events_funnel.h"

// Implements a set of methods to send infobar related events to the Broker.
class InfobarEventsFunnel : public EventsFunnel {
 public:
  InfobarEventsFunnel() {}

  // Sends the infobar.onDocumentComplete event to the Broker.
  virtual HRESULT OnDocumentComplete();

 private:
  DISALLOW_COPY_AND_ASSIGN(InfobarEventsFunnel);
};

#endif  // CEEE_IE_PLUGIN_BHO_INFOBAR_EVENTS_FUNNEL_H_
