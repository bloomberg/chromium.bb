// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/key_commands_provider.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#include "ios/chrome/browser/ui/commands/start_voice_search_command.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation KeyCommandsProvider

- (NSArray*)keyCommandsForConsumer:(id<KeyCommandsPlumbing>)consumer
                        dispatcher:
                            (id<ApplicationCommands, BrowserCommands>)dispatcher
                       editingText:(BOOL)editingText {
  __weak id<KeyCommandsPlumbing> weakConsumer = consumer;
  __weak id<ApplicationCommands, BrowserCommands> weakDispatcher = dispatcher;

  // Block to execute a command from the |tag|.
  void (^execute)(NSInteger) = ^(NSInteger tag) {
    [weakConsumer
        chromeExecuteCommand:[GenericChromeCommand commandWithTag:tag]];
  };

  // Block to have the tab model open the tab at |index|, if there is one.
  void (^focusTab)(NSUInteger) = ^(NSUInteger index) {
    [weakConsumer focusTabAtIndex:index];
  };

  const BOOL hasTabs = [consumer tabsCount] > 0;

  const BOOL useRTLLayout = UseRTLLayout();

  // Blocks for navigating forward/back.
  void (^browseLeft)();
  void (^browseRight)();
  if (useRTLLayout) {
    browseLeft = ^{
      if ([weakConsumer canGoForward])
        [weakDispatcher goForward];
    };
    browseRight = ^{
      if ([weakConsumer canGoBack])
        [weakDispatcher goBack];
    };
  } else {
    browseLeft = ^{
      if ([weakConsumer canGoBack])
        [weakDispatcher goBack];
    };
    browseRight = ^{
      if ([weakConsumer canGoForward])
        [weakDispatcher goForward];
    };
  }

  // New tab blocks.
  void (^newTab)() = ^{
    [weakDispatcher openNewTab:[OpenNewTabCommand command]];
  };

  void (^newIncognitoTab)() = ^{
    [weakDispatcher openNewTab:[OpenNewTabCommand incognitoTabCommand]];
  };

  const int browseLeftDescriptionID = useRTLLayout
                                          ? IDS_IOS_KEYBOARD_HISTORY_FORWARD
                                          : IDS_IOS_KEYBOARD_HISTORY_BACK;
  const int browseRightDescriptionID = useRTLLayout
                                           ? IDS_IOS_KEYBOARD_HISTORY_BACK
                                           : IDS_IOS_KEYBOARD_HISTORY_FORWARD;

  // Initialize the array of commands with an estimated capacity.
  NSMutableArray* keyCommands = [NSMutableArray arrayWithCapacity:32];

  // List the commands that always appear in the HUD. They appear in the HUD
  // since they have titles.
  [keyCommands addObjectsFromArray:@[
    [UIKeyCommand cr_keyCommandWithInput:@"t"
                           modifierFlags:UIKeyModifierCommand
                                   title:l10n_util::GetNSStringWithFixup(
                                             IDS_IOS_TOOLS_MENU_NEW_TAB)
                                  action:newTab],
    [UIKeyCommand
        cr_keyCommandWithInput:@"n"
                 modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                         title:l10n_util::GetNSStringWithFixup(
                                   IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB)
                        action:newIncognitoTab],
    [UIKeyCommand
        cr_keyCommandWithInput:@"t"
                 modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                         title:l10n_util::GetNSStringWithFixup(
                                   IDS_IOS_KEYBOARD_REOPEN_CLOSED_TAB)
                        action:^{
                          [weakConsumer reopenClosedTab];
                        }],
  ]];

  // List the commands that only appear when there is at least a tab. When they
  // appear, they are in the HUD since they have titles.
  if (hasTabs) {
    [keyCommands addObjectsFromArray:@[
      [UIKeyCommand cr_keyCommandWithInput:@"l"
                             modifierFlags:UIKeyModifierCommand
                                     title:l10n_util::GetNSStringWithFixup(
                                               IDS_IOS_KEYBOARD_OPEN_LOCATION)
                                    action:^{
                                      [weakConsumer focusOmnibox];
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"w"
                             modifierFlags:UIKeyModifierCommand
                                     title:l10n_util::GetNSStringWithFixup(
                                               IDS_IOS_TOOLS_MENU_CLOSE_TAB)
                                    action:^{
                                      [weakDispatcher closeCurrentTab];
                                    }],
      [UIKeyCommand
          cr_keyCommandWithInput:@"d"
                   modifierFlags:UIKeyModifierCommand
                           title:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_KEYBOARD_BOOKMARK_THIS_PAGE)
                          action:^{
                            [weakDispatcher bookmarkPage];
                          }],
      [UIKeyCommand cr_keyCommandWithInput:@"f"
                             modifierFlags:UIKeyModifierCommand
                                     title:l10n_util::GetNSStringWithFixup(
                                               IDS_IOS_TOOLS_MENU_FIND_IN_PAGE)
                                    action:^{
                                      execute(IDC_FIND);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"g"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      execute(IDC_FIND_NEXT);
                                    }],
      [UIKeyCommand
          cr_keyCommandWithInput:@"g"
                   modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                           title:nil
                          action:^{
                            execute(IDC_FIND_PREVIOUS);
                          }],
      [UIKeyCommand cr_keyCommandWithInput:@"r"
                             modifierFlags:UIKeyModifierCommand
                                     title:l10n_util::GetNSStringWithFixup(
                                               IDS_IOS_ACCNAME_RELOAD)
                                    action:^{
                                      [weakDispatcher reload];
                                    }],
    ]];

    // Since cmd+left and cmd+right are valid system shortcuts when editing
    // text, don't register those if text is being edited.
    if (!editingText) {
      [keyCommands addObjectsFromArray:@[
        [UIKeyCommand cr_keyCommandWithInput:UIKeyInputLeftArrow
                               modifierFlags:UIKeyModifierCommand
                                       title:l10n_util::GetNSStringWithFixup(
                                                 browseLeftDescriptionID)
                                      action:browseLeft],
        [UIKeyCommand cr_keyCommandWithInput:UIKeyInputRightArrow
                               modifierFlags:UIKeyModifierCommand
                                       title:l10n_util::GetNSStringWithFixup(
                                                 browseRightDescriptionID)
                                      action:browseRight],
      ]];
    }

    NSString* voiceSearchTitle = l10n_util::GetNSStringWithFixup(
        IDS_IOS_VOICE_SEARCH_KEYBOARD_DISCOVERY_TITLE);
    [keyCommands addObjectsFromArray:@[
      [UIKeyCommand cr_keyCommandWithInput:@"y"
                             modifierFlags:UIKeyModifierCommand
                                     title:l10n_util::GetNSStringWithFixup(
                                               IDS_HISTORY_SHOW_HISTORY)
                                    action:^{
                                      execute(IDC_SHOW_HISTORY);
                                    }],
      [UIKeyCommand
          cr_keyCommandWithInput:@"."
                   modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                           title:voiceSearchTitle
                          action:^{
                            StartVoiceSearchCommand* command =
                                [[StartVoiceSearchCommand alloc]
                                    initWithOriginView:nil];
                            [weakDispatcher startVoiceSearch:command];
                          }],
    ]];
  }

  // List the commands that don't appear in the HUD but are always present.
  [keyCommands addObjectsFromArray:@[
    [UIKeyCommand cr_keyCommandWithInput:UIKeyInputEscape
                           modifierFlags:Cr_UIKeyModifierNone
                                   title:nil
                                  action:^{
                                    execute(IDC_CLOSE_MODALS);
                                  }],
    [UIKeyCommand cr_keyCommandWithInput:@"n"
                           modifierFlags:UIKeyModifierCommand
                                   title:nil
                                  action:newTab],
    [UIKeyCommand cr_keyCommandWithInput:@","
                           modifierFlags:UIKeyModifierCommand
                                   title:nil
                                  action:^{
                                    [weakDispatcher showSettings];
                                  }],
  ]];

  // List the commands that don't appear in the HUD and only appear when there
  // is at least a tab.
  if (hasTabs) {
    [keyCommands addObjectsFromArray:@[
      [UIKeyCommand cr_keyCommandWithInput:@"["
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:browseLeft],
      [UIKeyCommand cr_keyCommandWithInput:@"]"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:browseRight],
      [UIKeyCommand cr_keyCommandWithInput:@"."
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      [weakDispatcher stopLoading];
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"?"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      execute(IDC_HELP_PAGE_VIA_MENU);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"1"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(0);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"2"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(1);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"3"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(2);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"4"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(3);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"5"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(4);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"6"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(5);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"7"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(6);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"8"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(7);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"9"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab([weakConsumer tabsCount] - 1);
                                    }],
      [UIKeyCommand
          cr_keyCommandWithInput:UIKeyInputLeftArrow
                   modifierFlags:UIKeyModifierCommand | UIKeyModifierAlternate
                           title:nil
                          action:^{
                            [weakConsumer focusPreviousTab];
                          }],
      [UIKeyCommand
          cr_keyCommandWithInput:UIKeyInputRightArrow
                   modifierFlags:UIKeyModifierCommand | UIKeyModifierAlternate
                           title:nil
                          action:^{
                            [weakConsumer focusNextTab];
                          }],
    ]];
  }

  return keyCommands;
}

@end
