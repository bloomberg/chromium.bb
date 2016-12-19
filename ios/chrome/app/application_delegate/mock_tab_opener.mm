// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/application_delegate/mock_tab_opener.h"

#include "base/ios/block_types.h"
#include "base/mac/scoped_block.h"
#include "ios/chrome/app/application_mode.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@interface MockTabOpener () {
  base::mac::ScopedBlock<ProceduralBlock> _completionBlock;
}

@end

@implementation MockTabOpener

@synthesize url = _url;
@synthesize applicationMode = _applicationMode;

- (void)dismissModalsAndOpenSelectedTabInMode:(ApplicationMode)targetMode
                                      withURL:(const GURL&)url
                                   transition:(ui::PageTransition)transition
                                   completion:(ProceduralBlock)handler {
  _url = url;
  _applicationMode = targetMode;
  _completionBlock.reset([handler copy]);
}

- (void)resetURL {
  _url = _url.EmptyGURL();
}

- (void (^)())completionBlock {
  return _completionBlock;
}

- (void)openTabFromLaunchOptions:(NSDictionary*)launchOptions
              startupInformation:(id<StartupInformation>)startupInformation
                        appState:(AppState*)appState {
  // Stub.
}

- (BOOL)shouldOpenNTPTabOnActivationOfTabModel:(TabModel*)tabModel {
  // Stub.
  return YES;
}

@end
