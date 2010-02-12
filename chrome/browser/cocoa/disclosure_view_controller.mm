// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/disclosure_view_controller.h"
#include "base/logging.h"
#include "base/scoped_nsobject.h"

const NSCellStateValue kInitialDisclosureState = NSOffState;
const NSInteger kClosedBoxHeight = 20;
NSString* const kKVODisclosedKey = @"disclosed";

// This class externalizes the state of the disclosure control.  When the
// disclosure control is pressed it changes the state of this object.  In turn
// the KVO machinery detects the change to |disclosed| and signals the
// |observeValueForKeyPath| call in the |DisclosureViewController|.
@interface DisclosureViewState : NSObject {
 @private
  NSCellStateValue disclosed;
}
@end

@implementation DisclosureViewState
@end

@interface DisclosureViewController(PrivateMethods)

- (void)initDisclosureState:(NSCellStateValue)state;
- (NSRect)openStateFrameSize:(NSRect)startFrame;
- (NSRect)closedStateFrameSize:(NSRect)startFrame;

- (void)startAnimations:(NSView*)view
                  start:(NSRect)startFrame
                    end:(NSRect)endFrame;

- (void)discloseDetails:(NSCellStateValue)state;

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context;

@end

@implementation DisclosureViewController

@synthesize disclosureState = disclosureState_;

- (void)awakeFromNib {
  // Create the disclosure state.
  [self setDisclosureState:[[[DisclosureViewState alloc] init] autorelease]];

  // Set up the initial disclosure state before we install the observer.
  // We don't want our animations firing before we're done initializing.
  [disclosureState_ setValue:[NSNumber numberWithInt:kInitialDisclosureState]
           forKey:kKVODisclosedKey];

  // Pick up "open" height from the initial state of the view in the nib.
  openHeight_ = [[self view] frame].size.height;

  // Set frame size according to initial disclosure state.
  [self initDisclosureState:kInitialDisclosureState];

  // Setup observers so that when disclosure state changes we resize frame
  // accordingly.
  [disclosureState_ addObserver:self forKeyPath:kKVODisclosedKey
             options:NSKeyValueObservingOptionNew|NSKeyValueObservingOptionOld
             context:nil];
}

- (void)dealloc {
  [disclosureState_ removeObserver:self forKeyPath:kKVODisclosedKey];
  [disclosureState_ release];
  [super dealloc];
}

@end

@implementation DisclosureViewController(PrivateMethods)

// Initializes the view's frame geometry based on the input |state|.
// If the |state| is NSOnState then the frame size corresponds to "open".
// If the |state| is NSOffState then the frame size corresponds to "closed".
// The |origin.x| and |size.width| remain unchanged, but the |origin.y| and
// |size.height| may vary.
- (void)initDisclosureState:(NSCellStateValue)state {
  if (state == NSOnState) {
    [[self view] setFrame:[self openStateFrameSize:[[self view] frame]]];
  }
  else if (state == NSOffState) {
    [[self view] setFrame:[self closedStateFrameSize:[[self view] frame]]];
  }
  else {
    NOTREACHED();
  }
}

// Computes the frame geometry during the "open" state of the disclosure view.
- (NSRect)openStateFrameSize:(NSRect)startFrame {
  return NSMakeRect(startFrame.origin.x,
                    startFrame.size.height - openHeight_ +
                        startFrame.origin.y,
                    startFrame.size.width,
                    openHeight_);
}

// Computes the frame geometry during the "closed" state of the disclosure view.
- (NSRect)closedStateFrameSize:(NSRect)startFrame {
  return NSMakeRect(startFrame.origin.x,
                    startFrame.size.height - kClosedBoxHeight +
                        startFrame.origin.y,
                    startFrame.size.width,
                    kClosedBoxHeight);
}

// Animates the opening or closing of the disclosure view.  The |startFrame|
// specifies the frame geometry at the beginning of the animation and the
// |endFrame| specifies the geometry at the end of the animation.  The input
// |view| is view managed by this controller.
- (void)startAnimations:(NSView*)view
                  start:(NSRect)startFrame
                    end:(NSRect)endFrame
{
  // Setup dictionary describing animation.
  // Create the attributes dictionary for the first view.
  NSMutableDictionary* dictionary;
  dictionary = [NSDictionary dictionaryWithObjectsAndKeys:
      // Specify which view to modify.
      view, NSViewAnimationTargetKey,
      // Specify the starting position of the view.
      [NSValue valueWithRect:startFrame], NSViewAnimationStartFrameKey,
      // Change the ending position of the view.
      [NSValue valueWithRect:endFrame], NSViewAnimationEndFrameKey,
      nil];

  // Create the view animation object.
  scoped_nsobject<NSViewAnimation> animation;
  animation.reset([[NSViewAnimation alloc] initWithViewAnimations:
               [NSArray arrayWithObject:dictionary]]);

  // Set some additional attributes for the animation.
  [animation.get() setDuration:.2];
  [animation.get() setAnimationCurve:NSAnimationEaseIn];

  // Run the animation.
  [animation.get() startAnimation];
}

// This method is invoked when the disclosure state changes.  It computes
// the appropriate view frame geometry and then initiates the animation to
// change that geometry.
- (void)discloseDetails:(NSCellStateValue)state {
  NSRect startFrame = [[self view] frame];
  NSRect endFrame = startFrame;

  if (state == NSOnState) {
    endFrame = [self openStateFrameSize:startFrame];
  } else if (state == NSOffState) {
    endFrame = [self closedStateFrameSize:startFrame];
  } else {
    NOTREACHED();
    return;
  }

  [self startAnimations:[self view] start:startFrame end:endFrame];
}

// The |DisclosureViewController| is an observer of an instance of a
// |DisclosureViewState| object.  This object lives within the controller's
// nib file.  When the KVO machinery detects a change to the state
// it triggers this call and we initiate the change in frame geometry of the
// view.
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:kKVODisclosedKey]) {
    NSCellStateValue newValue =
        [[change objectForKey:NSKeyValueChangeNewKey] intValue];
    NSCellStateValue oldValue =
        [[change objectForKey:NSKeyValueChangeOldKey] intValue];

    if (newValue != oldValue) {
      [self discloseDetails:newValue];
    }
  }
}

@end
