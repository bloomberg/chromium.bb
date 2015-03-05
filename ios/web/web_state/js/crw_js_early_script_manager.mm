// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/js/crw_js_early_script_manager.h"

#import "base/mac/scoped_nsobject.h"
#import "ios/web/public/web_state/js/crw_js_base_manager.h"
#include "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/js/crw_js_message_manager.h"

@interface CRWJSEarlyScriptManager () {
  base::scoped_nsobject<NSMutableSet> _dependencies;
}
@end

@implementation CRWJSEarlyScriptManager

#pragma mark - Protected Methods

- (id)initWithReceiver:(CRWJSInjectionReceiver*)receiver {
  self = [super initWithReceiver:receiver];
  if (self) {
    _dependencies.reset([[NSMutableSet alloc] initWithArray:@[
        // TODO(eugenebut): update with correct dependencies when they are
        // upstreamed.
        [CRWJSBaseManager class],
        [CRWJSMessageManager class],
    ]]);
  }
  return self;
}

- (NSString*)staticInjectionContent {
  // Early Script Manager does not supply any scripts of its own. Rather it uses
  // its dependencies to ensure all relevant script content is injected into
  // pages.
  return @"";
}

- (NSString*)presenceBeacon {
  return @"__gCrWeb";
}

- (NSArray*)directDependencies {
  return [_dependencies allObjects];
}

- (void)addDependency:(Class)scriptManager {
  [_dependencies addObject:scriptManager];
}

- (void)removeDependency:(Class)scriptManager {
  [_dependencies removeObject:scriptManager];
}

#pragma mark -

@end
