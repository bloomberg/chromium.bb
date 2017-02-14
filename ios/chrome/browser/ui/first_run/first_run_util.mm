// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/first_run/first_run_configuration.h"
#include "ios/chrome/browser/first_run/first_run_metrics.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/tabs/tab.h"
#include "ios/chrome/browser/ui/first_run/first_run_histograms.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#import "ios/chrome/browser/ui/sync/sync_util.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/web/public/web_thread.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kChromeFirstRunUIWillFinishNotification =
    @"kChromeFirstRunUIWillFinishNotification";

NSString* const kChromeFirstRunUIDidFinishNotification =
    @"kChromeFirstRunUIDidFinishNotification";

namespace {

NSString* RemoveLastWord(NSString* text) {
  __block NSRange range = NSMakeRange(0, [text length]);
  NSStringEnumerationOptions options = NSStringEnumerationByWords |
                                       NSStringEnumerationReverse |
                                       NSStringEnumerationSubstringNotRequired;

  // Enumerate backwards through the words in |text| to get the range of the
  // last word.
  [text
      enumerateSubstringsInRange:range
                         options:options
                      usingBlock:^(NSString* substring, NSRange substringRange,
                                   NSRange enclosingRange, BOOL* stop) {
                        range = substringRange;
                        *stop = YES;
                      }];
  return [text substringToIndex:range.location];
}

NSString* InsertNewlineBeforeNthToLastWord(NSString* text, int index) {
  __block NSRange range = NSMakeRange(0, [text length]);
  __block int count = 0;
  NSStringEnumerationOptions options = NSStringEnumerationByWords |
                                       NSStringEnumerationReverse |
                                       NSStringEnumerationSubstringNotRequired;
  [text
      enumerateSubstringsInRange:range
                         options:options
                      usingBlock:^(NSString* substring, NSRange substringRange,
                                   NSRange enclosingRange, BOOL* stop) {
                        range = substringRange;
                        count++;
                        *stop = count == index;
                      }];
  NSMutableString* textWithNewline = [text mutableCopy];
  [textWithNewline insertString:@"\n" atIndex:range.location];
  return textWithNewline;
}

// Trampoline method for Bind to create the sentinel file.
bool CreateSentinel() {
  return FirstRun::CreateSentinel();
}

// Helper function for recording first run metrics. Takes an additional
// |to_record| argument which is the returned value from CreateSentinel().
void RecordFirstRunMetricsInternal(ios::ChromeBrowserState* browserState,
                                   bool sign_in_attempted,
                                   bool has_sso_accounts,
                                   bool to_record) {
  // |to_record| is false if the sentinel file was not created which indicates
  // that the sentinel already exists and metrics were already recorded.
  // Note: If the user signs in and then signs out during first run, it will be
  // recorded as a successful sign in.
  if (!to_record)
    return;

  bool user_signed_in =
      ios::SigninManagerFactory::GetForBrowserState(browserState)
          ->IsAuthenticated();
  first_run::SignInStatus sign_in_status;

  if (user_signed_in) {
    sign_in_status = has_sso_accounts
                         ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SUCCESSFUL
                         : first_run::SIGNIN_SUCCESSFUL;
  } else {
    if (sign_in_attempted) {
      sign_in_status = has_sso_accounts
                           ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_GIVEUP
                           : first_run::SIGNIN_SKIPPED_GIVEUP;
    } else {
      sign_in_status = has_sso_accounts
                           ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_QUICK
                           : first_run::SIGNIN_SKIPPED_QUICK;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("FirstRun.SignIn", sign_in_status,
                            first_run::SIGNIN_SIZE);
}

}  // namespace

namespace ios_internal {

BOOL FixOrphanWord(UILabel* label) {
  // Calculate the height of the label's text.
  NSString* text = label.text;
  CGSize textSize =
      [text cr_boundingSizeWithSize:label.frame.size font:label.font];
  CGFloat textHeight = AlignValueToPixel(textSize.height);

  // Remove the last word and calculate the height of the new text.
  NSString* textMinusLastWord = RemoveLastWord(text);
  CGSize minusLastWordSize =
      [textMinusLastWord cr_boundingSizeWithSize:label.frame.size
                                            font:label.font];
  CGFloat minusLastWordHeight = AlignValueToPixel(minusLastWordSize.height);

  // Check if removing the last word results in a smaller height.
  if (minusLastWordHeight < textHeight) {
    // The last word was the only word on its line. Add a newline before the
    // second to last word.
    label.text = InsertNewlineBeforeNthToLastWord(text, 2);
    return true;
  }
  return false;
}

void WriteFirstRunSentinelAndRecordMetrics(
    ios::ChromeBrowserState* browserState,
    BOOL sign_in_attempted,
    BOOL has_sso_account) {
  // Call CreateSentinel() and pass the result into RecordFirstRunMetrics().
  base::Callback<bool(void)> task = base::Bind(&CreateSentinel);
  base::Callback<void(bool)> reply =
      base::Bind(&RecordFirstRunMetricsInternal, browserState,
                 sign_in_attempted, has_sso_account);
  base::PostTaskAndReplyWithResult(web::WebThread::GetBlockingPool(), FROM_HERE,
                                   task, reply);
}

void FinishFirstRun(ios::ChromeBrowserState* browserState,
                    Tab* tab,
                    FirstRunConfiguration* config) {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kChromeFirstRunUIWillFinishNotification
                    object:nil];
  WriteFirstRunSentinelAndRecordMetrics(browserState, config.signInAttempted,
                                        config.hasSSOAccount);

  // Display the sync errors infobar.
  ios_internal::sync::displaySyncErrors(browserState, tab);
}

void RecordProductTourTimingMetrics(NSString* timer_name,
                                    base::TimeTicks start_time) {
  base::TimeDelta delta = base::TimeTicks::Now() - start_time;
  NSString* histogramName =
      [NSString stringWithFormat:@"ProductTour.IOSScreens%@", timer_name];
  UMA_HISTOGRAM_CUSTOM_TIMES_FIRST_RUN(base::SysNSStringToUTF8(histogramName),
                                       delta,
                                       base::TimeDelta::FromMilliseconds(10),
                                       base::TimeDelta::FromMinutes(3), 50);
}

void FirstRunDismissed() {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kChromeFirstRunUIDidFinishNotification
                    object:nil];
}

}  // namespace ios_internal
