// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_application_mac.h"

#include "base/auto_reset.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "googleurl/src/gurl.h"

@implementation ShellCrApplication

- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)sendEvent:(NSEvent*)event {
  AutoReset<BOOL> scoper(&handlingSendEvent_, YES);
  [super sendEvent:event];
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (IBAction)newDocument:(id)sender {
  content::Shell::CreateNewWindow(content::ShellBrowserContext::GetInstance(),
                                  GURL("about:blank"),
                                  NULL,
                                  MSG_ROUTING_NONE,
                                  NULL);
}

@end
