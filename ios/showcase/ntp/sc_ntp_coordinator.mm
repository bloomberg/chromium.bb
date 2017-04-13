// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/ntp/sc_ntp_coordinator.h"

#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_item.h"
#import "ios/clean/chrome/browser/ui/commands/ntp_commands.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_view_controller.h"
#import "ios/showcase/common/protocol_alerter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCNTPCoordinator ()
@property(nonatomic, strong) ProtocolAlerter* alerter;
@end

@implementation SCNTPCoordinator

@synthesize baseViewController;
@synthesize alerter = _alerter;

- (void)start {
  NTPViewController* ntp = [[NTPViewController alloc] init];

  self.alerter =
      [[ProtocolAlerter alloc] initWithProtocols:@[ @protocol(NTPCommands) ]];
  self.alerter.baseViewController = self.baseViewController;
  ntp.dispatcher = static_cast<id<NTPCommands>>(self.alerter);

  NewTabPageBarItem* item1 = [NewTabPageBarItem
      newTabPageBarItemWithTitle:@"Item 1"
                      identifier:0
                           image:[UIImage imageNamed:@"ntp_mv_search"]];
  NewTabPageBarItem* item2 = [NewTabPageBarItem
      newTabPageBarItemWithTitle:@"Item 2"
                      identifier:0
                           image:[UIImage imageNamed:@"ntp_bookmarks"]];
  [ntp setBarItems:@[ item1, item2 ]];

  [self.baseViewController pushViewController:ntp animated:YES];
}

@end
