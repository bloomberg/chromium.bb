// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/first_run_bubble_controller.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#include "components/search_engines/util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

@interface FirstRunBubbleController(Private)
- (id)initRelativeToView:(NSView*)view
                  offset:(NSPoint)offset
                 browser:(Browser*)browser
                 profile:(Profile*)profile;
- (void)closeIfNotKey;
@end

@implementation FirstRunBubbleController

+ (FirstRunBubbleController*) showForView:(NSView*)view
                                   offset:(NSPoint)offset
                                  browser:(Browser*)browser
                                  profile:(Profile*)profile {
  // Autoreleases itself on bubble close.
  return [[FirstRunBubbleController alloc] initRelativeToView:view
                                                       offset:offset
                                                      browser:browser
                                                      profile:profile];
}

- (id)initRelativeToView:(NSView*)view
                  offset:(NSPoint)offset
                 browser:(Browser*)browser
                 profile:(Profile*)profile {
  if ((self = [super initWithWindowNibPath:@"FirstRunBubble"
                            relativeToView:view
                                    offset:offset])) {
    browser_ = browser;
    profile_ = profile;
    [self showWindow:nil];

    // On 10.5, the first run bubble sometimes does not disappear when clicking
    // the omnibox. This happens if the bubble never became key, due to it
    // showing up so early in the startup sequence. As a workaround, close it
    // automatically after a few seconds if it doesn't become key.
    // http://crbug.com/52726
    [self performSelector:@selector(closeIfNotKey) withObject:nil afterDelay:3];
  }
  return self;
}

- (void)awakeFromNib {
  first_run::LogFirstRunMetric(first_run::FIRST_RUN_BUBBLE_SHOWN);

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  DCHECK(header_);
  [header_ setStringValue:cocoa_l10n_util::ReplaceNSStringPlaceholders(
      [header_ stringValue], GetDefaultSearchEngineName(service), NULL)];

  // Adapt window size to contents. Do this before all other layouting.
  CGFloat dy = cocoa_l10n_util::VerticallyReflowGroup([[self bubble] subviews]);
  NSSize ds = NSMakeSize(0, dy);
  ds = [[self bubble] convertSize:ds toView:nil];

  NSRect frame = [[self window] frame];
  frame.origin.y -= ds.height;
  frame.size.height += ds.height;
  [[self window] setFrame:frame display:YES];
}

- (void)close {
  // If the window is closed before the timer is fired, cancel the timer, since
  // it retains the controller.
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(closeIfNotKey)
                                             object:nil];
  [super close];
}

- (void)closeIfNotKey {
  if (![[self window] isKeyWindow])
    [self close];
}

- (IBAction)onChange:(id)sender {
  first_run::LogFirstRunMetric(first_run::FIRST_RUN_BUBBLE_CHANGE_INVOKED);

  Browser* browser = browser_;
  [self close];
  if (browser)
    chrome::ShowSearchEngineSettings(browser);
}

@end  // FirstRunBubbleController
