// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/print_observer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Prefix for print JavaScript command.
const char kPrintCommandPrefix[] = "print";
}

PrintObserver::PrintObserver(web::WebState* web_state,
                             id<BrowserCommands> dispatcher)
    : web::WebStateObserver(web_state), dispatcher_(dispatcher) {
  web_state->AddScriptCommandCallback(
      base::Bind(&PrintObserver::OnPrintCommand, base::Unretained(this)),
      kPrintCommandPrefix);
}

PrintObserver::~PrintObserver() {
  Detach();
}

void PrintObserver::WebStateDestroyed() {
  Detach();
}

bool PrintObserver::OnPrintCommand(const base::DictionaryValue&,
                                   const GURL&,
                                   bool) {
  [dispatcher_ printTab];
  return true;
}

void PrintObserver::Detach() {
  if (web_state()) {
    web_state()->RemoveScriptCommandCallback(kPrintCommandPrefix);
  }
}
