// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_window.h"

#include <list>
#include <memory>

#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sessions/NSCoder+Compatibility.h"
#import "ios/chrome/browser/sessions/session_service.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/public/web_thread.h"
#import "ios/web/web_state/web_state_impl.h"

using web::WebStateImpl;

// Serialization keys.
NSString* const kSessionsKey = @"sessions";
NSString* const kSelectedIndexKey = @"selectedIndex";

@interface SessionWindowIOS () {
  // Backing objects for properties of the same name.
  base::scoped_nsobject<NSMutableArray> _sessions;
  NSUInteger _selectedIndex;
}

// Returns whether |index| is valid for a session window with |sessionCount|
// entries.
- (BOOL)isIndex:(NSUInteger)index validForSessionCount:(NSUInteger)sessionCount;

@end

@implementation SessionWindowIOS

@synthesize selectedIndex = _selectedIndex;

- (id)init {
  if ((self = [super init])) {
    _sessions.reset([[NSMutableArray alloc] init]);
    _selectedIndex = NSNotFound;
  }
  return self;
}

#pragma mark - Accessors

- (NSArray*)sessions {
  return [NSArray arrayWithArray:_sessions];
}

- (void)setSelectedIndex:(NSUInteger)selectedIndex {
  DCHECK([self isIndex:selectedIndex validForSessionCount:[_sessions count]]);
  _selectedIndex = selectedIndex;
}

#pragma mark - Public

- (void)addSerializedSessionStorage:(CRWSessionStorage*)session {
  [_sessions addObject:session];
  // Set the selected index to 0 (this session) if this is the first session
  // added.
  if (_selectedIndex == NSNotFound)
    _selectedIndex = 0;
}

- (void)clearSessions {
  [_sessions removeAllObjects];
  _selectedIndex = NSNotFound;
}

#pragma mark - NSCoding

- (id)initWithCoder:(NSCoder*)aDecoder {
  self = [super init];
  if (self) {
    DCHECK([aDecoder isKindOfClass:[SessionWindowUnarchiver class]]);
    _selectedIndex = [aDecoder cr_decodeIndexForKey:kSelectedIndexKey];
    _sessions.reset([[aDecoder decodeObjectForKey:kSessionsKey] retain]);
    DCHECK(
        [self isIndex:_selectedIndex validForSessionCount:[_sessions count]]);
    // If index is somehow corrupted, reset it to zero.
    // (note that if |_selectedIndex| == |_sessions.size()|, that's
    // incorrect because the maximum index of a vector of size |n| is |n-1|).
    // Empty sessions should have |_selectedIndex| values of NSNotFound.
    if (![_sessions count]) {
      _selectedIndex = NSNotFound;
    } else if (_selectedIndex >= [_sessions count]) {
      _selectedIndex = 0;
    }
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder cr_encodeIndex:_selectedIndex forKey:kSelectedIndexKey];
  [aCoder encodeObject:_sessions forKey:kSessionsKey];
}

#pragma mark -

- (BOOL)isIndex:(NSUInteger)index
    validForSessionCount:(NSUInteger)sessionCount {
  return (sessionCount && index < sessionCount) ||
         (!sessionCount && index == NSNotFound);
}

- (NSString*)description {
  return [NSString stringWithFormat:@"selected index: %" PRIuNS
                                     "\nsessions:\n%@\n",
                                    _selectedIndex, _sessions.get()];
}

@end
