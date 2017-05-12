// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/fake_mailto_handler_helpers.h"

@implementation FakeMailtoHandlerGmailNotInstalled
- (BOOL)isAvailable {
  return NO;
}
@end

@implementation FakeMailtoHandlerGmailInstalled
- (BOOL)isAvailable {
  return YES;
}
@end

@implementation CountingMailtoURLRewriterObserver
@synthesize changeCount = _changeCount;
- (void)rewriterDidChange:(MailtoURLRewriter*)rewriter {
  ++_changeCount;
}
@end
