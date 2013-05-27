// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/prescient_networking_dispatcher.h"

#include "chrome/common/render_messages.h"
#include "content/public/renderer/render_thread.h"

using WebKit::WebPrescientNetworking;

PrescientNetworkingDispatcher::~PrescientNetworkingDispatcher() {
}

void PrescientNetworkingDispatcher::preconnect(
    const WebKit::WebURL& url,
    WebKit::WebPreconnectMotivation motivation) {
  // FIXME(kouhei) actual pre-connecting is currently disabled.
  // This should shortly be enabled via Finch field trials in upcoming patch.
  // content::RenderThread::Get()->Send(new ChromeViewHostMsg_Preconnect(url));
}

