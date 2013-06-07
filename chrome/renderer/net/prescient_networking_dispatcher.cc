// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/prescient_networking_dispatcher.h"

#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "content/public/renderer/render_thread.h"

using WebKit::WebPrescientNetworking;

const char kMouseEventPreconnectFieldTrialName[] = "MouseEventPreconnect";
const char kMouseEventPreconnectFieldTrialMouseDownGroup[] = "MouseDown";
const char kMouseEventPreconnectFieldTrialMouseOverGroup[] = "MouseOver";
const char kMouseEventPreconnectFieldTrialTapUnconfirmedGroup[] =
    "TapUnconfirmed";
const char kMouseEventPreconnectFieldTrialTapDownGroup[] = "TapDown";

namespace {

// Returns true if preconnect is enabled for given motivation.
// The preconnect via {mouse,gesture} event is enabled for limited userbase
// for Finch field trial.
bool isPreconnectEnabledForMotivation(
    WebKit::WebPreconnectMotivation motivation) {
  std::string group =
      base::FieldTrialList::FindFullName(kMouseEventPreconnectFieldTrialName);

  switch (motivation) {
    case WebKit::WebPreconnectMotivationLinkMouseDown:
      return group == kMouseEventPreconnectFieldTrialMouseDownGroup;
    case WebKit::WebPreconnectMotivationLinkMouseOver:
      return group == kMouseEventPreconnectFieldTrialMouseOverGroup;
    case WebKit::WebPreconnectMotivationLinkTapUnconfirmed:
      return group == kMouseEventPreconnectFieldTrialTapUnconfirmedGroup;
    case WebKit::WebPreconnectMotivationLinkTapDown:
      return group == kMouseEventPreconnectFieldTrialTapDownGroup;
    default:
      return false;
  }
}

} // namespace

PrescientNetworkingDispatcher::PrescientNetworkingDispatcher() {
}

PrescientNetworkingDispatcher::~PrescientNetworkingDispatcher() {
}

void PrescientNetworkingDispatcher::prefetchDNS(
    const WebKit::WebString& hostname) {
  if (hostname.isEmpty())
    return;

  std::string hostname_utf8 = UTF16ToUTF8(hostname);
  net_predictor_.Resolve(hostname_utf8.data(), hostname_utf8.length());
}

void PrescientNetworkingDispatcher::preconnect(
    const WebKit::WebURL& url,
    WebKit::WebPreconnectMotivation motivation) {
  if (isPreconnectEnabledForMotivation(motivation))
    content::RenderThread::Get()->Send(new ChromeViewHostMsg_Preconnect(url));
}

