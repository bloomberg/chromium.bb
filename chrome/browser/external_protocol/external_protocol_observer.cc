// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_observer.h"

#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "content/browser/tab_contents/tab_contents.h"

ExternalProtocolObserver::ExternalProtocolObserver(TabContents* tab_contents)
    : TabContentsObserver(tab_contents) {
}

ExternalProtocolObserver::~ExternalProtocolObserver() {
}

void ExternalProtocolObserver::DidGetUserGesture() {
  ExternalProtocolHandler::PermitLaunchUrl();
}
