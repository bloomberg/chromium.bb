// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/external_app_launcher_tab_helper.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/external_app/open_mail_handler_view_controller.h"
#import "ios/chrome/browser/web/external_app_launcher_util.h"
#import "ios/chrome/browser/web/external_apps_launch_policy_decider.h"
#import "ios/chrome/browser/web/mailto_handler.h"
#import "ios/chrome/browser/web/mailto_handler_manager.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/third_party/material_components_ios/src/components/BottomSheet/src/MDCBottomSheetController.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(ExternalAppLauncherTabHelper);

namespace {

// Launches the mail client app represented by |handler| and records metrics.
void LaunchMailClientApp(const GURL& url, MailtoHandler* handler) {
  NSString* launch_url = [handler rewriteMailtoURL:url];
  UMA_HISTOGRAM_BOOLEAN("IOS.MailtoURLRewritten", launch_url != nil);
  NSURL* url_to_open = launch_url.length ? [NSURL URLWithString:launch_url]
                                         : net::NSURLWithGURL(url);
  if (@available(iOS 10, *)) {
    [[UIApplication sharedApplication] openURL:url_to_open
                                       options:@{}
                             completionHandler:nil];
  }
#if !defined(__IPHONE_10_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0
  else {
    [[UIApplication sharedApplication] openURL:url_to_open];
  }
#endif
}

// Shows a prompt for the user to choose which mail client app to use to handle
// a mailto:// URL.
void PromptForMailClientWithUrl(const GURL& url,
                                MailtoHandlerManager* manager) {
  GURL copied_url_to_open = url;
  OpenMailHandlerViewController* mail_handler_chooser =
      [[OpenMailHandlerViewController alloc]
          initWithManager:manager
          selectedHandler:^(MailtoHandler* _Nonnull handler) {
            LaunchMailClientApp(copied_url_to_open, handler);
          }];
  MDCBottomSheetController* bottom_sheet = [[MDCBottomSheetController alloc]
      initWithContentViewController:mail_handler_chooser];
  [[[[UIApplication sharedApplication] keyWindow] rootViewController]
      presentViewController:bottom_sheet
                   animated:YES
                 completion:nil];
}

// Presents an alert controller on the root view controller with |prompt| as
// body text, |accept label| and |reject label| as button labels, and
// a non null |responseHandler| that takes a boolean to handle user response.
void ShowExternalAppLauncherPrompt(NSString* prompt,
                                   NSString* accept_label,
                                   NSString* reject_label,
                                   base::OnceCallback<void(bool)> callback) {
  __block base::OnceCallback<void(bool)> block_callback = std::move(callback);
  UIAlertController* alert_controller =
      [UIAlertController alertControllerWithTitle:nil
                                          message:prompt
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* accept_action =
      [UIAlertAction actionWithTitle:accept_label
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action) {
                               std::move(block_callback).Run(true);
                             }];
  UIAlertAction* reject_action =
      [UIAlertAction actionWithTitle:reject_label
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action) {
                               std::move(block_callback).Run(false);
                             }];
  [alert_controller addAction:reject_action];
  [alert_controller addAction:accept_action];

  [[[[UIApplication sharedApplication] keyWindow] rootViewController]
      presentViewController:alert_controller
                   animated:YES
                 completion:nil];
}

// Launches external app identified by |url| if |accept| is true.
void LaunchExternalApp(NSURL* url, bool accept) {
  UMA_HISTOGRAM_BOOLEAN("Tab.ExternalApplicationOpened", accept);
  if (accept) {
    [[UIApplication sharedApplication] openURL:url
                                       options:@{}
                             completionHandler:nil];
  }
}

// Presents an alert controller with |prompt| and |open_label| as button label
// on the root view controller before launching an external app identified by
// |url|.
void OpenExternalAppWithUrl(NSURL* url,
                            NSString* prompt,
                            NSString* open_label) {
  ShowExternalAppLauncherPrompt(
      prompt, /*accept_label=*/open_label,
      /*reject_label=*/l10n_util::GetNSString(IDS_CANCEL),
      base::BindOnce(&LaunchExternalApp, url));
}

// Opens URL in an external application if possible (optionally after
// confirming via dialog in case that user didn't interact using
// |link_clicked| or if the external application is face time) or returns NO
// if there is no such application available.
bool OpenUrl(const GURL& gurl, bool link_clicked) {
  // Don't open external application if chrome is not active.
  if ([[UIApplication sharedApplication] applicationState] !=
      UIApplicationStateActive) {
    return NO;
  }

  NSURL* url = net::NSURLWithGURL(gurl);
  if (@available(iOS 10.3, *)) {
    if (UrlHasAppStoreScheme(gurl)) {
      NSString* prompt = l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP);
      NSString* open_label =
          l10n_util::GetNSString(IDS_IOS_APP_LAUNCHER_OPEN_APP_BUTTON_LABEL);
      OpenExternalAppWithUrl(url, prompt, open_label);
      return true;
    }
  } else {
    // Prior to iOS 10.3, iOS does not prompt user when facetime: and
    // facetime-audio: URL schemes are opened, so Chrome needs to present an
    // alert before placing a phone call.
    if (UrlHasPhoneCallScheme(gurl)) {
      OpenExternalAppWithUrl(
          url, /*prompt=*/GetFormattedAbsoluteUrlWithSchemeRemoved(url),
          /*open_label=*/GetPromptActionString(url.scheme));
      return true;
    }
    // Prior to iOS 10.3, Chrome prompts user with an alert before opening
    // App Store when user did not tap on any links and an iTunes app URL is
    // opened. This maintains parity with Safari in pre-10.3 environment.
    if (!link_clicked && UrlHasAppStoreScheme(gurl)) {
      NSString* prompt = l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP);
      NSString* open_label =
          l10n_util::GetNSString(IDS_IOS_APP_LAUNCHER_OPEN_APP_BUTTON_LABEL);
      OpenExternalAppWithUrl(url, prompt, open_label);
      return true;
    }
  }

  // Replaces |url| with a rewritten URL if it is of mailto: scheme.
  if (gurl.SchemeIs(url::kMailToScheme)) {
    MailtoHandlerManager* manager =
        [MailtoHandlerManager mailtoHandlerManagerWithStandardHandlers];
    NSString* handler_id = manager.defaultHandlerID;
    if (!handler_id) {
      PromptForMailClientWithUrl(gurl, manager);
      return true;
    }
    MailtoHandler* handler = [manager defaultHandlerByID:handler_id];
    LaunchMailClientApp(gurl, handler);
    return true;
  }

// If the following call returns YES, an external application is about to be
// launched and Chrome will go into the background now.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  // TODO(crbug.com/774736): This call still needs to be
  // updated. It's heavily nested so some refactoring is needed.
  return [[UIApplication sharedApplication] openURL:url];
#pragma clang diagnostic pop
}

}  // namespace

ExternalAppLauncherTabHelper::ExternalAppLauncherTabHelper(
    web::WebState* web_state)
    : policy_decider_([[ExternalAppsLaunchPolicyDecider alloc] init]),
      weak_factory_(this) {}

ExternalAppLauncherTabHelper::~ExternalAppLauncherTabHelper() = default;

void ExternalAppLauncherTabHelper::HandleRepeatedAttemptsToLaunch(
    const GURL& url,
    const GURL& source_page_url,
    bool allowed) {
  if (allowed) {
    // By confirming that user wants to launch the
    // application, there is no need to check for
    // |link_clicked|.
    OpenUrl(url, /*link_clicked=*/true);
  } else {
    // TODO(crbug.com/674649): Once non modal
    // dialogs are implemented, update this to
    // always prompt instead of blocking the app.
    [policy_decider_ blockLaunchingAppURL:url
                        fromSourcePageURL:source_page_url];
  }
  UMA_HISTOGRAM_BOOLEAN("IOS.RepeatedExternalAppPromptResponse", allowed);
  is_prompt_active_ = false;
}

bool ExternalAppLauncherTabHelper::RequestToOpenUrl(const GURL& url,
                                                    const GURL& source_page_url,
                                                    bool link_clicked) {
  if (!url.is_valid() || !url.has_scheme())
    return false;

  // Don't open external application if chrome is not active.
  if ([[UIApplication sharedApplication] applicationState] !=
      UIApplicationStateActive) {
    return false;
  }

  // Don't try to open external application if a prompt is already active.
  if (is_prompt_active_)
    return false;

  [policy_decider_ didRequestLaunchExternalAppURL:url
                                fromSourcePageURL:source_page_url];
  ExternalAppLaunchPolicy policy =
      [policy_decider_ launchPolicyForURL:url
                        fromSourcePageURL:source_page_url];
  switch (policy) {
    case ExternalAppLaunchPolicyBlock: {
      return false;
    }
    case ExternalAppLaunchPolicyAllow: {
      return OpenUrl(url, link_clicked);
    }
    case ExternalAppLaunchPolicyPrompt: {
      is_prompt_active_ = true;
      NSString* prompt_body =
          l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP);
      NSString* allow_label =
          l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP_ALLOW);
      NSString* block_label =
          l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP_BLOCK);

      base::OnceCallback<void(bool)> callback = base::BindOnce(
          &ExternalAppLauncherTabHelper::HandleRepeatedAttemptsToLaunch,
          weak_factory_.GetWeakPtr(), url, source_page_url);

      ShowExternalAppLauncherPrompt(prompt_body, allow_label, block_label,
                                    std::move(callback));
      return true;
    }
  }
}
