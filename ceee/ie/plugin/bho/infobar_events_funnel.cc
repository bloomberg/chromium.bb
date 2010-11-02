// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Infobar Events to the Broker.

#include "ceee/ie/plugin/bho/infobar_events_funnel.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "base/json/json_writer.h"

namespace {
const char kOnDocumentCompleteEventName[] = "infobar.onDocumentComplete";
}

HRESULT InfobarEventsFunnel::OnDocumentComplete() {
  DictionaryValue info;
  return SendEvent(kOnDocumentCompleteEventName, info);
}
