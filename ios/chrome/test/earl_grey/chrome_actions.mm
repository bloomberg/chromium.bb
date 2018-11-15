// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_actions.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* kChromeActionsErrorDomain = @"ChromeActionsError";
}  // namespace

namespace chrome_test_util {

id<GREYAction> LongPressElementForContextMenu(
    web::test::ElementSelector selector,
    bool triggers_context_menu) {
  return WebViewLongPressElementForContextMenu(
      chrome_test_util::GetCurrentWebState(), std::move(selector),
      triggers_context_menu);
}

id<GREYAction> TurnSettingsSwitchOn(BOOL on) {
  id<GREYMatcher> constraints = grey_not(grey_systemAlertViewShown());
  NSString* action_name =
      [NSString stringWithFormat:@"Turn settings switch to %@ state",
                                 on ? @"ON" : @"OFF"];
  return [GREYActionBlock
      actionWithName:action_name
         constraints:constraints
        performBlock:^BOOL(id collection_view_cell,
                           __strong NSError** error_or_nil) {
          SettingsSwitchCell* switch_cell =
              base::mac::ObjCCast<SettingsSwitchCell>(collection_view_cell);
          if (!switch_cell) {
            *error_or_nil = [NSError
                errorWithDomain:kChromeActionsErrorDomain
                           code:0
                       userInfo:@{
                         NSLocalizedDescriptionKey : @"The element isn't of "
                                                     @"the expected type "
                                                     @"(SettingsSwitchCell)."
                       }];
            return NO;
          }
          UISwitch* switch_view = switch_cell.switchView;
          if (switch_view.on != on) {
            id<GREYAction> long_press_action = [GREYActions
                actionForLongPressWithDuration:kGREYLongPressDefaultDuration];
            return [long_press_action perform:switch_view error:error_or_nil];
          }
          return YES;
        }];
}

id<GREYAction> TurnLegacySettingsSwitchOn(BOOL on) {
  id<GREYMatcher> constraints = grey_not(grey_systemAlertViewShown());
  NSString* action_name =
      [NSString stringWithFormat:@"Turn settings switch to %@ state",
                                 on ? @"ON" : @"OFF"];
  return [GREYActionBlock
      actionWithName:action_name
         constraints:constraints
        performBlock:^BOOL(id collection_view_cell,
                           __strong NSError** error_or_nil) {
          LegacySettingsSwitchCell* switch_cell =
              base::mac::ObjCCast<LegacySettingsSwitchCell>(
                  collection_view_cell);
          if (!switch_cell) {
            *error_or_nil =
                [NSError errorWithDomain:kChromeActionsErrorDomain
                                    code:0
                                userInfo:@{
                                  NSLocalizedDescriptionKey :
                                      @"The element isn't of the expected type "
                                      @"(LegacySettingsSwitchCell)."
                                }];
            return NO;
          }
          UISwitch* switch_view = switch_cell.switchView;
          if (switch_view.on != on) {
            id<GREYAction> long_press_action = [GREYActions
                actionForLongPressWithDuration:kGREYLongPressDefaultDuration];
            return [long_press_action perform:switch_view error:error_or_nil];
          }
          return YES;
        }];
}

id<GREYAction> TurnSyncSwitchOn(BOOL on) {
  id<GREYMatcher> constraints = grey_not(grey_systemAlertViewShown());
  NSString* actionName = [NSString
      stringWithFormat:@"Turn sync switch to %@ state", on ? @"ON" : @"OFF"];
  return [GREYActionBlock
      actionWithName:actionName
         constraints:constraints
        performBlock:^BOOL(id sync_switch_cell,
                           __strong NSError** error_or_nil) {
          SyncSwitchCell* switch_cell =
              base::mac::ObjCCastStrict<SyncSwitchCell>(sync_switch_cell);
          UISwitch* switch_view = switch_cell.switchView;
          if (switch_view.on != on) {
            id<GREYAction> long_press_action = [GREYActions
                actionForLongPressWithDuration:kGREYLongPressDefaultDuration];
            return [long_press_action perform:switch_view error:error_or_nil];
          }
          return YES;
        }];
}

id<GREYAction> TapWebElement(const std::string& element_id) {
  return web::WebViewTapElement(
      chrome_test_util::GetCurrentWebState(),
      web::test::ElementSelector::ElementSelectorId(element_id));
}

id<GREYAction> TapWebElementInFrame(const std::string& element_id,
                                    const int frame_index) {
  return web::WebViewTapElement(
      chrome_test_util::GetCurrentWebState(),
      web::test::ElementSelector::ElementSelectorIdInFrame(element_id,
                                                           frame_index));
}

}  // namespace chrome_test_util
