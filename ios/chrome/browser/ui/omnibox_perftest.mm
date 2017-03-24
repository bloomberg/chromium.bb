// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#import "base/test/ios/wait_util.h"
#include "base/time/time.h"
#include "components/toolbar/test_toolbar_model.h"
#include "ios/chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_view_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_delegate_ios.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_impl_ios.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#include "ios/chrome/test/base/perf_test_ios.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/test/ios/keyboard_appearance_listener.h"

namespace {

// Descends down a view hierarchy until the first view of |specificClass|
// is found. Returns nil if a view of |specificClass| cannot be found.
UIView* FindViewByClass(UIView* topView, Class specificClass) {
  if ([topView isKindOfClass:specificClass])
    return [topView window] ? topView : nil;
  for (UIView* subview in [topView subviews]) {
    UIView* foundView = FindViewByClass(subview, specificClass);
    if (foundView)
      return foundView;
  }
  return nil;
}

// Constant for UI wait loop.
const NSTimeInterval kSpinDelaySeconds = 0.01;

class OmniboxPerfTest : public PerfTest {
 public:
  OmniboxPerfTest() : PerfTest("Omnibox") {}

 protected:
  void SetUp() override {
    PerfTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::AutocompleteClassifierFactory::GetInstance(),
        ios::AutocompleteClassifierFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();

    // Prepares testing profile for autocompletion.
    ios::AutocompleteClassifierFactory::GetForBrowserState(
        chrome_browser_state_.get());

    // Sets up the listener for keyboard activation/deactivation notifications.
    keyboard_listener_.reset([[KeyboardAppearanceListener alloc] init]);

    // Create a real window to host the Toolbar.
    CGRect screenBounds = [[UIScreen mainScreen] bounds];
    window_.reset([[UIWindow alloc] initWithFrame:screenBounds]);
    [window_ makeKeyAndVisible];

    // Create a mock Tab and a TabModel that will always return the mock Tab as
    // the current tab.
    current_tab_.reset(
        [(Tab*)[OCMockObject niceMockForClass:[Tab class]] retain]);
    id tab_model = [OCMockObject mockForClass:[TabModel class]];
    [[[tab_model stub] andReturn:current_tab_] currentTab];
    tab_model_.reset([tab_model retain]);

    // Creates the Toolbar for testing and sizes it to the width of the screen.
    toolbar_model_delegate_.reset(
        new ToolbarModelDelegateIOS(tab_model_.get()));
    toolbar_model_ios_.reset(
        new ToolbarModelImplIOS(toolbar_model_delegate_.get()));

    // The OCMOCK_VALUE macro doesn't like std::unique_ptr, but it works just
    // fine if a temporary variable is used.
    ToolbarModelIOS* model_for_mock = toolbar_model_ios_.get();
    id webToolbarDelegate =
        [OCMockObject niceMockForProtocol:@protocol(WebToolbarDelegate)];
    [[[webToolbarDelegate stub] andReturnValue:OCMOCK_VALUE(model_for_mock)]
        toolbarModelIOS];
    id urlLoader = [OCMockObject niceMockForProtocol:@protocol(UrlLoader)];
    toolbar_.reset([[WebToolbarController alloc]
        initWithDelegate:webToolbarDelegate
               urlLoader:urlLoader
            browserState:chrome_browser_state_.get()
         preloadProvider:nil]);
    UIView* toolbarView = [toolbar_ view];
    CGRect toolbarFrame = toolbarView.frame;
    toolbarFrame.origin = CGPointZero;
    toolbarFrame.size.width = screenBounds.size.width;
    toolbarView.frame = toolbarFrame;
    // Add toolbar to window.
    [window_ addSubview:toolbarView];
    base::test::ios::WaitUntilCondition(^bool() {
      return IsToolbarLoaded(window_.get());
    });
  }

  void TearDown() override {
    // Remove toolbar from window.
    [[toolbar_ view] removeFromSuperview];
    base::test::ios::WaitUntilCondition(^bool() {
      return !IsToolbarLoaded(window_.get());
    });
    [toolbar_ browserStateDestroyed];
    PerfTest::TearDown();
  }

  // Returns whether Omnibox has been loaded.
  bool IsToolbarLoaded(UIView* topView) {
    return FindViewByClass(topView, [OmniboxTextFieldIOS class]) != nil;
  }

  // Inserts the |text| string into the |textField| and return the amount
  // of time it took to complete the insertion. This does not time
  // any activities that may be triggered on other threads.
  base::TimeDelta TimeInsertText(OmniboxTextFieldIOS* textField,
                                 NSString* text) {
    base::Time startTime = base::Time::NowFromSystemTime();
    [textField insertTextWhileEditing:text];
    base::TimeDelta elapsed = base::Time::NowFromSystemTime() - startTime;
    // Adds a small delay so the run loop can update the screen.
    // The user experience measurement should include this visual
    // feedback to the user, but there is no way of catching when
    // the typed character showed up in the omnibox on screen.
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::TimeDelta::FromSecondsD(kSpinDelaySeconds));
    return elapsed;
  }

  // Creates a dummy text field and make it be a first responder so keyboard
  // comes up. In general, there's a lot of time spent in loading up various
  // graphics assets on a keyboard which may distort the measurement of Chrome
  // Omnibox focus timings. Call this function to preload keyboard before
  // doing the real test.
  base::TimeDelta PreLoadKeyboard() {
    base::scoped_nsobject<UITextField> textField(
        [[UITextField alloc] initWithFrame:CGRectMake(20, 100, 280, 29)]);
    [textField setBorderStyle:UITextBorderStyleRoundedRect];
    [window_ addSubview:textField];
    base::TimeDelta elapsed = base::test::ios::TimeUntilCondition(
        ^{
          [textField becomeFirstResponder];
        },
        ^bool() {
          return [keyboard_listener_ isKeyboardVisible];
        },
        false, base::TimeDelta());
    base::test::ios::TimeUntilCondition(
        ^{
          [textField resignFirstResponder];
        },
        ^bool() {
          return ![keyboard_listener_ isKeyboardVisible];
        },
        false, base::TimeDelta());
    [textField removeFromSuperview];
    return elapsed;
  }

  // Enables the on-screen keyboard as if user has tapped on |textField|.
  // Returns the amount of time it took for the keyboard to appear.
  base::TimeDelta EnableKeyboard(OmniboxTextFieldIOS* textField) {
    return base::test::ios::TimeUntilCondition(
        ^{
          [textField becomeFirstResponder];
        },
        ^bool() {
          return [keyboard_listener_ isKeyboardVisible];
        },
        false, base::TimeDelta());
  }

  // Performs necessary cleanup (so next pass of unit test can start from
  // a clean slate) and then exit from |textField| to dismiss keyboard.
  void DisableKeyboard(OmniboxTextFieldIOS* textField) {
    // Busy wait until keyboard is hidden.
    base::test::ios::TimeUntilCondition(
        ^{
          [textField exitPreEditState];
          [textField resignFirstResponder];
        },
        ^bool() {
          return ![keyboard_listener_ isKeyboardVisible];
        },
        false, base::TimeDelta());
  }

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::scoped_nsobject<Tab> current_tab_;
  base::scoped_nsobject<TabModel> tab_model_;
  std::unique_ptr<ToolbarModelDelegateIOS> toolbar_model_delegate_;
  std::unique_ptr<ToolbarModelIOS> toolbar_model_ios_;
  base::scoped_nsobject<WebToolbarController> toolbar_;
  base::scoped_nsobject<UIWindow> window_;
  base::scoped_nsobject<KeyboardAppearanceListener> keyboard_listener_;
};

// Measures the amount of time it takes the Omnibox text field to activate
// the on-screen keyboard.
TEST_F(OmniboxPerfTest, TestTextFieldDidBeginEditing) {
  LogPerfTiming("Keyboard preload", PreLoadKeyboard());
  OmniboxTextFieldIOS* textField = (OmniboxTextFieldIOS*)FindViewByClass(
      [toolbar_ view], [OmniboxTextFieldIOS class]);

  // Time how long it takes to "focus" on omnibox.
  RepeatTimedRuns("Begin editing",
                  ^base::TimeDelta(int index) {
                    return EnableKeyboard(textField);
                  },
                  ^() {
                    DisableKeyboard(textField);
                  });
}

// Measures the amount of time it takes to type in the first character
// into the Omnibox.
TEST_F(OmniboxPerfTest, TestTypeOneCharInTextField) {
  OmniboxTextFieldIOS* textField = (OmniboxTextFieldIOS*)FindViewByClass(
      [toolbar_ view], [OmniboxTextFieldIOS class]);
  RepeatTimedRuns("Type first character",
                  ^base::TimeDelta(int index) {
                    EnableKeyboard(textField);
                    return TimeInsertText(textField, @"G");
                  },
                  ^() {
                    [textField setText:@""];
                    DisableKeyboard(textField);
                  });
}

// Measures the amount of time it takes to type in the word "google" one
// letter at a time.
TEST_F(OmniboxPerfTest, TestTypingInTextField) {
  OmniboxTextFieldIOS* textField = (OmniboxTextFieldIOS*)FindViewByClass(
      [toolbar_ view], [OmniboxTextFieldIOS class]);
  // The characters to type into the omnibox text field.
  NSArray* inputCharacters =
      [NSArray arrayWithObjects:@"g", @"o", @"o", @"g", @"l", @"e", nil];
  RepeatTimedRuns(
      "Typing",
      ^base::TimeDelta(int index) {
        EnableKeyboard(textField);
        NSMutableString* logMessage = [NSMutableString string];
        base::TimeDelta elapsed;
        for (NSString* input in inputCharacters) {
          base::TimeDelta inputElapsed = TimeInsertText(textField, input);
          [logMessage appendFormat:@"%@'%@':%.0f",
                                   [logMessage length] ? @" " : @"", input,
                                   inputElapsed.InMillisecondsF()];
          elapsed += inputElapsed;
        }
        NSLog(@"%2d: %@", index, logMessage);
        return elapsed;
      },
      ^() {
        [textField setText:@""];
        DisableKeyboard(textField);
      });
}
}
