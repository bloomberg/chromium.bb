// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_window.h"

#include <list>
#include <memory>

#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sessions/NSCoder+Compatibility.h"
#import "ios/chrome/browser/sessions/session_service.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/public/web_thread.h"
#import "ios/web/web_state/web_state_impl.h"

using web::WebStateImpl;

@interface SessionWindowIOS () {
 @private
  NSUInteger _selectedIndex;  // Currently selected session.
  // For SessionWindows created via -initWithSessions:currentIndex:, the
  // WebStateImpls in |_sessions| are owned by the calling code. When created
  // via -initwithCoder:, the code which accepts the SessionWindow object
  // should take or assign ownership of the contents of |_sessions|.
  std::list<web::WebStateImpl*> _sessions;
}

// For testing only. Empties _sessions.
- (void)clearSessions;

@end

@implementation SessionWindowIOS

@synthesize selectedIndex = _selectedIndex;

- (id)init {
  if ((self = [super init])) {
    _selectedIndex = NSNotFound;
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)aDecoder {
  self = [super init];
  if (self) {
    DCHECK([aDecoder isKindOfClass:[SessionWindowUnarchiver class]]);
    ios::ChromeBrowserState* browserState =
        static_cast<SessionWindowUnarchiver*>(aDecoder).browserState;
    DCHECK(browserState);
    _selectedIndex = [aDecoder cr_decodeIndexForKey:@"selectedIndex"];
    base::scoped_nsobject<NSArray> decodedSessionControllers(
        [[aDecoder decodeObjectForKey:@"sessions"] retain]);
    for (CRWSessionController* sc in decodedSessionControllers.get()) {
      WebStateImpl* webState = new WebStateImpl(browserState);
      webState->GetNavigationManagerImpl().SetSessionController(sc);
      _sessions.push_back(webState);
    }
    DCHECK((_sessions.size() && _selectedIndex < _sessions.size()) ||
           (_sessions.empty() && _selectedIndex == NSNotFound));
    // If index is somehow corrupted, reset it to zero.
    // (note that if |_selectedIndex| == |_sessions.size()|, that's
    // incorrect because the maximum index of a vector of size |n| is |n-1|).
    // Empty sessions should have |_selectedIndex| values of NSNotFound.
    if (_sessions.empty()) {
      _selectedIndex = NSNotFound;
    } else if (_selectedIndex >= _sessions.size()) {
      _selectedIndex = 0;
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK(_sessions.empty());
  [super dealloc];
}

- (void)clearSessions {
  while (self.unclaimedSessions) {
    std::unique_ptr<WebStateImpl> webState = [self nextSession];
    webState.reset();
  }
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  // Destructively pull all of the WebStateImpls out of |_sessions| and hand
  // off their sessionControllers for archiving. When encoding is complete,
  // all of the objects in |_sessions| have been destroyed.
  NSMutableArray* sessionControllers =
      [NSMutableArray arrayWithCapacity:_sessions.size()];
  while (self.unclaimedSessions) {
    std::unique_ptr<WebStateImpl> webState = [self nextSession];
    CRWSessionController* sessionController =
        webState->GetNavigationManagerImpl().GetSessionController();
    [sessionControllers addObject:sessionController];

    // NOTE: WebStateImpl must be destroyed on the UI thread for safety
    // reasons.
    web::WebThread::DeleteSoon(web::WebThread::UI, FROM_HERE,
                               webState.release());
  }

  [aCoder cr_encodeIndex:_selectedIndex forKey:@"selectedIndex"];
  [aCoder encodeObject:sessionControllers forKey:@"sessions"];
}

- (void)addSession:(std::unique_ptr<web::WebStateImpl>)session {
  DCHECK(session->GetNavigationManagerImpl().GetSessionController());
  _sessions.push_back(session.release());
  // Set the selected index to 0 (this session) if this is the first session
  // added.
  if (_selectedIndex == NSNotFound)
    _selectedIndex = 0;
}

- (void)setSelectedIndex:(NSUInteger)selectedIndex {
  DCHECK((_sessions.size() && selectedIndex < _sessions.size()) ||
         (_sessions.empty() && selectedIndex == NSNotFound));
  _selectedIndex = selectedIndex;
}

- (std::unique_ptr<web::WebStateImpl>)nextSession {
  std::unique_ptr<web::WebStateImpl> session(_sessions.front());
  _sessions.pop_front();
  return session;
}

- (NSUInteger)unclaimedSessions {
  return _sessions.size();
}

#pragma mark -
#pragma mark Debugging conveniences.

- (NSString*)description {
  NSMutableArray* sessionControllers =
      [NSMutableArray arrayWithCapacity:_sessions.size()];
  for (auto it = _sessions.begin(); it != _sessions.end(); ++it) {
    CRWSessionController* sessionController =
        (*it)->GetNavigationManagerImpl().GetSessionController();
    [sessionControllers addObject:sessionController];
  }

  return [NSString stringWithFormat:@"selected index: %" PRIuNS
                                     "\nsessions:\n%@\n",
                                    _selectedIndex, sessionControllers];
}

@end
