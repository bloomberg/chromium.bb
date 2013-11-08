// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/theme_helper_mac.h"

#include <Foundation/Foundation.h>

#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

@interface ScrollbarPrefsObserver : NSObject

+ (void)registerAsObserver;
+ (void)appearancePrefsChanged:(NSNotification*)notification;
+ (void)behaviorPrefsChanged:(NSNotification*)notification;
+ (void)notifyPrefsChangedWithRedraw:(BOOL)redraw;

@end

@implementation ScrollbarPrefsObserver

+ (void)registerAsObserver {
  [[NSDistributedNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(appearancePrefsChanged:)
             name:@"AppleAquaScrollBarVariantChanged"
           object:nil
suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];

  [[NSDistributedNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(behaviorPrefsChanged:)
             name:@"AppleNoRedisplayAppearancePreferenceChanged"
           object:nil
suspensionBehavior:NSNotificationSuspensionBehaviorCoalesce];
}

+ (void)appearancePrefsChanged:(NSNotification*)notification {
  [self notifyPrefsChangedWithRedraw:YES];
}

+ (void)behaviorPrefsChanged:(NSNotification*)notification {
  [self notifyPrefsChangedWithRedraw:NO];
}

+ (void)notifyPrefsChangedWithRedraw:(BOOL)redraw {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults synchronize];

  content::ThemeHelperMac::SendThemeChangeToAllRenderers(
      [defaults floatForKey:@"NSScrollerButtonDelay"],
      [defaults floatForKey:@"NSScrollerButtonPeriod"],
      [defaults boolForKey:@"AppleScrollerPagingBehavior"],
      redraw);
}

@end

namespace content {

ThemeHelperMac::ThemeHelperMac() {
  [ScrollbarPrefsObserver registerAsObserver];
  registrar_.Add(this,
                 NOTIFICATION_RENDERER_PROCESS_CREATED,
                 NotificationService::AllSources());
}

ThemeHelperMac::~ThemeHelperMac() {
}

// static
ThemeHelperMac* ThemeHelperMac::GetInstance() {
  return Singleton<ThemeHelperMac,
      LeakySingletonTraits<ThemeHelperMac> >::get();
}


void ThemeHelperMac::Observe(int type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK_EQ(NOTIFICATION_RENDERER_PROCESS_CREATED, type);

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults synchronize];

  RenderProcessHost* rph = Source<RenderProcessHost>(source).ptr();
  rph->Send(new ViewMsg_UpdateScrollbarTheme(
      [defaults floatForKey:@"NSScrollerButtonDelay"],
      [defaults floatForKey:@"NSScrollerButtonPeriod"],
      [defaults boolForKey:@"AppleScrollerPagingBehavior"],
      false));
}

// static
void ThemeHelperMac::SendThemeChangeToAllRenderers(
    float initial_button_delay,
    float autoscroll_button_delay,
    bool jump_on_track_click,
    bool redraw) {
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd();
       it.Advance()) {
    it.GetCurrentValue()->Send(new ViewMsg_UpdateScrollbarTheme(
        initial_button_delay,
        autoscroll_button_delay,
        jump_on_track_click,
        redraw));
  }
}

}  // namespace content
