// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_controller.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/form_parsing/ios_form_parser.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/js_password_manager.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/passwords/credential_manager.h"
#include "ios/chrome/browser/passwords/credential_manager_features.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/ios_chrome_update_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/notify_auto_signin_view_controller.h"
#import "ios/chrome/browser/passwords/password_form_filler.h"
#import "ios/chrome/browser/ssl/insecure_input_tab_helper.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#include "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/origin_util.h"
#include "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormData;
using autofill::PasswordForm;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::GetPageURLAndCheckTrustLevel;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordManager;
using password_manager::PasswordManagerClient;
using password_manager::PasswordManagerDriver;
using password_manager::SerializeFillData;
using password_manager::SerializePasswordFormFillData;

typedef void (^PasswordSuggestionsAvailableCompletion)(
    const AccountSelectFillData*);

namespace {
// Types of password infobars to display.
enum class PasswordInfoBarType { SAVE, UPDATE };

// Types of password suggestion in the keyboard accessory. Used for metrics
// collection.
enum class PasswordSuggestionType {
  // Credentials are listed.
  CREDENTIALS = 0,
  // Only "Show All" is listed.
  SHOW_ALL = 1,
  COUNT
};

// Duration for notify user auto-sign in dialog being displayed.
constexpr int kNotifyAutoSigninDuration = 3;  // seconds

// The string ' •••' appended to the username in the suggestion.
NSString* const kSuggestionSuffix = @" ••••••••";

void LogSuggestionClicked(PasswordSuggestionType type) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.SuggestionClicked", type,
                            PasswordSuggestionType::COUNT);
}

void LogSuggestionShown(PasswordSuggestionType type) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.SuggestionShown", type,
                            PasswordSuggestionType::COUNT);
}
}  // namespace

@interface PasswordController ()

// View controller for auto sign-in notification, owned by this
// PasswordController.
@property(nonatomic, strong)
    NotifyUserAutoSigninViewController* notifyAutoSigninViewController;

// Helper contains common password controller logic.
@property(nonatomic, readonly) PasswordControllerHelper* helper;

@end

@interface PasswordController ()<FormSuggestionProvider, PasswordFormFiller>

// Informs the |_passwordManager| of the password forms (if any were present)
// that have been found on the page.
- (void)didFinishPasswordFormExtraction:
    (const std::vector<autofill::PasswordForm>&)forms;

// Finds all password forms in DOM and sends them to the password store for
// fetching stored credentials.
- (void)findPasswordFormsAndSendThemToPasswordStore;

// Displays infobar for |form| with |type|. If |type| is UPDATE, the user
// is prompted to update the password. If |type| is SAVE, the user is prompted
// to save the password.
- (void)showInfoBarForForm:(std::unique_ptr<PasswordFormManagerForUI>)form
               infoBarType:(PasswordInfoBarType)type;

// Hides auto sign-in notification. Removes the view from superview and destroys
// the controller.
// TODO(crbug.com/435048): Animate disappearance.
- (void)hideAutosigninNotification;

@end

namespace {

// Constructs an array of FormSuggestions, each corresponding to a username/
// password pair in |AccountSelectFillData|. "Show all" item is appended.
NSArray* BuildSuggestions(const AccountSelectFillData& fillData,
                          NSString* formName,
                          NSString* fieldIdentifier,
                          NSString* fieldType) {
  base::string16 form_name = base::SysNSStringToUTF16(formName);
  base::string16 field_identifier = base::SysNSStringToUTF16(fieldIdentifier);
  bool is_password_field = [fieldType isEqualToString:@"password"];

  NSMutableArray* suggestions = [NSMutableArray array];
  PasswordSuggestionType suggestion_type = PasswordSuggestionType::SHOW_ALL;
  if (fillData.IsSuggestionsAvailable(form_name, field_identifier,
                                      is_password_field)) {
    std::vector<password_manager::UsernameAndRealm> username_and_realms_ =
        fillData.RetrieveSuggestions(form_name, field_identifier,
                                     is_password_field);

    // Add credentials.
    for (const auto& username_and_realm : username_and_realms_) {
      NSString* value = [base::SysUTF16ToNSString(username_and_realm.username)
          stringByAppendingString:kSuggestionSuffix];
      NSString* origin =
          username_and_realm.realm.empty()
              ? nil
              : base::SysUTF8ToNSString(username_and_realm.realm);

      [suggestions addObject:[FormSuggestion suggestionWithValue:value
                                              displayDescription:origin
                                                            icon:nil
                                                      identifier:0]];
      suggestion_type = PasswordSuggestionType::CREDENTIALS;
    }
  }

  // Add "Show all".
  NSString* showAll = l10n_util::GetNSString(IDS_IOS_SHOW_ALL_PASSWORDS);
  [suggestions addObject:[FormSuggestion suggestionWithValue:showAll
                                          displayDescription:nil
                                                        icon:nil
                                                  identifier:1]];
  LogSuggestionShown(suggestion_type);

  return [suggestions copy];
}

}  // namespace

@implementation PasswordController {
  std::unique_ptr<PasswordManager> _passwordManager;
  std::unique_ptr<PasswordManagerClient> _passwordManagerClient;
  std::unique_ptr<PasswordManagerDriver> _passwordManagerDriver;
  std::unique_ptr<CredentialManager> _credentialManager;

  AccountSelectFillData _fillData;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Timer for hiding "Signing in as ..." notification.
  base::OneShotTimer _notifyAutoSigninTimer;

  // True indicates that a request for credentials has been sent to the password
  // store.
  BOOL _sentRequestToStore;

  // The completion to inform FormSuggestionController that suggestions are
  // available for a given form and field.
  PasswordSuggestionsAvailableCompletion _suggestionsAvailableCompletion;

  // User credential waiting to be displayed in autosign-in snackbar, once tab
  // becomes active.
  std::unique_ptr<autofill::PasswordForm> _pendingAutoSigninPasswordForm;
}

@synthesize baseViewController = _baseViewController;

@synthesize dispatcher = _dispatcher;

@synthesize delegate = _delegate;

@synthesize notifyAutoSigninViewController = _notifyAutoSigninViewController;

@synthesize helper = _helper;

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [self initWithWebState:webState
                         client:nullptr];
  return self;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                          client:(std::unique_ptr<PasswordManagerClient>)
                                     passwordManagerClient {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _helper = [[PasswordControllerHelper alloc] initWithWebState:webState
                                                        delegate:self];
    if (passwordManagerClient)
      _passwordManagerClient = std::move(passwordManagerClient);
    else
      _passwordManagerClient.reset(new IOSChromePasswordManagerClient(self));
    _passwordManager.reset(new PasswordManager(_passwordManagerClient.get()));
    _passwordManagerDriver.reset(new IOSChromePasswordManagerDriver(self));

    _sentRequestToStore = NO;

    if (base::FeatureList::IsEnabled(features::kCredentialManager)) {
      _credentialManager = std::make_unique<CredentialManager>(
          _passwordManagerClient.get(), _webState);
    }
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

#pragma mark - Properties

- (id<PasswordFormFiller>)passwordFormFiller {
  return self;
}

- (PasswordManagerClient*)passwordManagerClient {
  return _passwordManagerClient.get();
}

- (PasswordManagerDriver*)passwordManagerDriver {
  return _passwordManagerDriver.get();
}

#pragma mark - PasswordFormFiller

- (void)findAndFillPasswordForms:(NSString*)username
                        password:(NSString*)password
               completionHandler:(void (^)(BOOL))completionHandler {
  [self.helper findAndFillPasswordFormsWithUserName:username
                                           password:password
                                  completionHandler:completionHandler];
}

#pragma mark - CRWWebStateObserver

// If Tab was shown, and there is a pending PasswordForm, display autosign-in
// notification.
- (void)webStateWasShown:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_pendingAutoSigninPasswordForm) {
    [self showAutosigninNotification:std::move(_pendingAutoSigninPasswordForm)];
    _pendingAutoSigninPasswordForm.reset();
  }
}

// If Tab was hidden, hide auto sign-in notification.
- (void)webStateWasHidden:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self hideAutosigninNotification];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  // Clear per-page state.
  _fillData.Reset();
  _sentRequestToStore = NO;
  _suggestionsAvailableCompletion = nil;

  // Retrieve the identity of the page. In case the page might be malicous,
  // returns early.
  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webState, &pageURL))
    return;

  if (!web::UrlHasWebScheme(pageURL))
    return;

  // Notify the password manager that the page loaded so it can clear its own
  // per-page state.
  self.passwordManager->DidNavigateMainFrame();

  if (!webState->ContentIsHTML()) {
    // If the current page is not HTML, it does not contain any HTML forms.
    [self
        didFinishPasswordFormExtraction:std::vector<autofill::PasswordForm>()];
  }

  [self findPasswordFormsAndSendThemToPasswordStore];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
  _passwordManagerDriver.reset();
  _passwordManager.reset();
  _passwordManagerClient.reset();
  _credentialManager.reset();
}

#pragma mark - FormSuggestionProvider

- (id<FormSuggestionProvider>)suggestionProvider {
  return self;
}

- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                                 fieldName:(NSString*)fieldName
                           fieldIdentifier:(NSString*)fieldIdentifier
                                 fieldType:(NSString*)fieldType
                                      type:(NSString*)type
                                typedValue:(NSString*)typedValue
                               isMainFrame:(BOOL)isMainFrame
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  if (!GetPageURLAndCheckTrustLevel(webState, nullptr)) {
    completion(NO);
    return;
  }

  bool should_send_request_to_store =
      !_sentRequestToStore && [type isEqual:@"focus"];
  bool is_password_field = [fieldType isEqual:@"password"];

  if (should_send_request_to_store) {
    // Save the completion and go look for suggestions.
    _suggestionsAvailableCompletion =
        [^(const AccountSelectFillData* fill_data) {
          if (is_password_field) {
            // Always display "Show all" on the password field.
            completion(YES);
          } else if (!fill_data) {
            completion(NO);
          } else {
            completion(fill_data->IsSuggestionsAvailable(
                base::SysNSStringToUTF16(formName),
                base::SysNSStringToUTF16(fieldIdentifier), false));
          }
        } copy];
    [self findPasswordFormsAndSendThemToPasswordStore];
    return;
  }

  completion(is_password_field ||
             _fillData.IsSuggestionsAvailable(
                 base::SysNSStringToUTF16(formName),
                 base::SysNSStringToUTF16(fieldIdentifier), false));
}

- (void)retrieveSuggestionsForForm:(NSString*)formName
                         fieldName:(NSString*)fieldName2
                   fieldIdentifier:(NSString*)fieldIdentifier
                         fieldType:(NSString*)fieldType
                              type:(NSString*)type
                        typedValue:(NSString*)typedValue
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  DCHECK(GetPageURLAndCheckTrustLevel(webState, nullptr));
  completion(BuildSuggestions(_fillData, formName, fieldIdentifier, fieldType),
             self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                  fieldName:(NSString*)fieldName2
            fieldIdentifier:(NSString*)fieldIdentifier
                       form:(NSString*)formName
          completionHandler:(SuggestionHandledCompletion)completion {
  if (suggestion.identifier == 1) {
    // Navigate to the settings list.
    [self.delegate displaySavedPasswordList];
    completion();
    LogSuggestionClicked(PasswordSuggestionType::SHOW_ALL);
    return;
  }
  LogSuggestionClicked(PasswordSuggestionType::CREDENTIALS);
  base::string16 value = base::SysNSStringToUTF16(suggestion.value);
  DCHECK([suggestion.value hasSuffix:kSuggestionSuffix]);
  value.erase(value.length() - kSuggestionSuffix.length);
  std::unique_ptr<FillData> fillData = _fillData.GetFillData(value);

  if (!fillData)
    completion();

  [self.helper fillPasswordFormWithFillData:*fillData
                          completionHandler:^(BOOL success) {
                            completion();
                          }];
}

#pragma mark - PasswordManagerClientDelegate

- (ios::ChromeBrowserState*)browserState {
  return _webState ? ios::ChromeBrowserState::FromBrowserState(
                         _webState->GetBrowserState())
                   : nullptr;
}

- (PasswordManager*)passwordManager {
  return _passwordManager.get();
}

- (const GURL&)lastCommittedURL {
  return self.helper.lastCommittedURL;
}

- (void)showSavePasswordInfoBar:
    (std::unique_ptr<PasswordFormManagerForUI>)formToSave {
  [self showInfoBarForForm:std::move(formToSave)
               infoBarType:PasswordInfoBarType::SAVE];
}

- (void)showUpdatePasswordInfoBar:
    (std::unique_ptr<PasswordFormManagerForUI>)formToUpdate {
  [self showInfoBarForForm:std::move(formToUpdate)
               infoBarType:PasswordInfoBarType::UPDATE];
}

// Shows auto sign-in notification and schedules hiding it after 3 seconds.
// TODO(crbug.com/435048): Animate appearance.
- (void)showAutosigninNotification:
    (std::unique_ptr<autofill::PasswordForm>)formSignedIn {
  if (!_webState)
    return;

  // If a notification is already being displayed, hides the old one, then shows
  // the new one.
  if (self.notifyAutoSigninViewController) {
    _notifyAutoSigninTimer.Stop();
    [self hideAutosigninNotification];
  }

  // Creates view controller then shows the subview.
  self.notifyAutoSigninViewController = [
      [NotifyUserAutoSigninViewController alloc]
      initWithUsername:base::SysUTF16ToNSString(formSignedIn->username_value)
               iconURL:formSignedIn->icon_url
      URLLoaderFactory:_webState->GetBrowserState()
                           ->GetSharedURLLoaderFactory()];
  TabIdTabHelper* tabIdHelper = TabIdTabHelper::FromWebState(_webState);
  if (![_delegate displaySignInNotification:self.notifyAutoSigninViewController
                                  fromTabId:tabIdHelper->tab_id()]) {
    // The notification was not shown. Store the password form in
    // |_pendingAutoSigninPasswordForm| to show the notification later.
    _pendingAutoSigninPasswordForm = std::move(formSignedIn);
    self.notifyAutoSigninViewController = nil;
    return;
  }

  // Hides notification after 3 seconds.
  __weak PasswordController* weakSelf = self;
  _notifyAutoSigninTimer.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kNotifyAutoSigninDuration),
      base::BindRepeating(^{
        [weakSelf hideAutosigninNotification];
      }));
}

#pragma mark - PasswordManagerDriverDelegate

- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
       completionHandler:(void (^)(BOOL))completionHandler {
  _fillData.Add(formData);

  if (_suggestionsAvailableCompletion) {
    _suggestionsAvailableCompletion(&_fillData);
    _suggestionsAvailableCompletion = nil;
  }

  [self.helper fillPasswordForm:formData completionHandler:completionHandler];
}

- (void)onNoSavedCredentials {
  if (_suggestionsAvailableCompletion)
    _suggestionsAvailableCompletion(nullptr);
  _suggestionsAvailableCompletion = nil;
}

#pragma mark - PasswordControllerHelperDelegate

- (void)helper:(PasswordControllerHelper*)helper
    didSubmitForm:(const PasswordForm&)form
      inMainFrame:(BOOL)inMainFrame {
  if (inMainFrame) {
    self.passwordManager->OnPasswordFormSubmitted(self.passwordManagerDriver,
                                                  form);
  } else {
    // Show a save prompt immediately because for iframes it is very hard to
    // figure out correctness of password forms submission.
    self.passwordManager->OnPasswordFormSubmittedNoChecks(
        self.passwordManagerDriver, form);
  }
}

#pragma mark - Private methods

- (void)didFinishPasswordFormExtraction:
    (const std::vector<autofill::PasswordForm>&)forms {
  // Do nothing if |self| has been detached.
  if (!self.passwordManager)
    return;

  if (!forms.empty()) {
    // Notify web_state about password forms, so that this can be taken into
    // account for the security state.
    if (_webState && !web::IsOriginSecure(_webState->GetLastCommittedURL())) {
      InsecureInputTabHelper::GetOrCreateForWebState(_webState)
          ->DidShowPasswordFieldInInsecureContext();
    }

    _sentRequestToStore = YES;
    // Invoke the password manager callback to autofill password forms
    // on the loaded page.
    self.passwordManager->OnPasswordFormsParsed(self.passwordManagerDriver,
                                                forms);
  } else {
    [self onNoSavedCredentials];
  }
  // Invoke the password manager callback to check if password was
  // accepted or rejected. If accepted, infobar is presented. If
  // rejected, the provisionally saved password is deleted. On Chrome
  // w/ a renderer, it is the renderer who calls OnPasswordFormsParsed()
  // and OnPasswordFormsRendered(). Bling has to improvised a bit on the
  // ordering of these two calls.
  self.passwordManager->OnPasswordFormsRendered(self.passwordManagerDriver,
                                                forms, true);
}

- (void)findPasswordFormsAndSendThemToPasswordStore {
  // Read all password forms from the page and send them to the password
  // manager.
  __weak PasswordController* weakSelf = self;
  [self.helper findPasswordFormsWithCompletionHandler:^(
                   const std::vector<autofill::PasswordForm>& forms) {
    [weakSelf didFinishPasswordFormExtraction:forms];
  }];
}

- (void)showInfoBarForForm:(std::unique_ptr<PasswordFormManagerForUI>)form
               infoBarType:(PasswordInfoBarType)type {
  if (!_webState)
    return;

  bool isSmartLockBrandingEnabled = false;
  if (self.browserState) {
    syncer::SyncService* sync_service =
        ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
    isSmartLockBrandingEnabled =
        password_bubble_experiment::IsSmartLockUser(sync_service);
  }
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(_webState);

  switch (type) {
    case PasswordInfoBarType::SAVE:
      IOSChromeSavePasswordInfoBarDelegate::Create(
          isSmartLockBrandingEnabled, infoBarManager, std::move(form),
          self.dispatcher);
      break;

    case PasswordInfoBarType::UPDATE:
      IOSChromeUpdatePasswordInfoBarDelegate::Create(
          isSmartLockBrandingEnabled, infoBarManager, std::move(form),
          self.baseViewController, self.dispatcher);
      break;
  }
}

- (void)hideAutosigninNotification {
  [self.notifyAutoSigninViewController willMoveToParentViewController:nil];
  [self.notifyAutoSigninViewController.view removeFromSuperview];
  [self.notifyAutoSigninViewController removeFromParentViewController];
  self.notifyAutoSigninViewController = nil;
}

@end
