// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/print_observer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#import "base/mac/scoped_nsobject.h"
#include "base/values.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/web/public/web_state/web_state.h"

namespace {
// Prefix for print JavaScript command.
const char kPrintCommandPrefix[] = "print";
}

PrintObserver::PrintObserver(web::WebState* web_state)
    : web::WebStateObserver(web_state) {
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
  base::scoped_nsobject<GenericChromeCommand> print_command(
      [[GenericChromeCommand alloc] initWithTag:IDC_PRINT]);
  [web_state()->GetView() chromeExecuteCommand:print_command];
  return true;
}

void PrintObserver::Detach() {
  if (web_state()) {
    web_state()->RemoveScriptCommandCallback(kPrintCommandPrefix);
  }
}
