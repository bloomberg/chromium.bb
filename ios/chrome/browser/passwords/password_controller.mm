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
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/form_parsing/ios_form_parser.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/js_password_manager.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/metrics/ukm_url_recorder.h"
#include "ios/chrome/browser/passwords/credential_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/ios_chrome_update_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/notify_auto_signin_view_controller.h"
#import "ios/chrome/browser/passwords/password_form_filler.h"
#include "ios/chrome/browser/passwords/password_manager_features.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/origin_util.h"
#include "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "ios/web/public/web_state/web_frame.h"
#include "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormData;
using autofill::NewPasswordFormGenerationData;
using autofill::PasswordForm;
using base::SysNSStringToUTF16;
using base::SysUTF16ToNSString;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::GetPageURLAndCheckTrustLevel;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordGenerationFrameHelper;
using password_manager::PasswordManager;
using password_manager::PasswordManagerClient;
using password_manager::PasswordManagerDriver;
using password_manager::SerializeFillData;
using password_manager::SerializePasswordFormFillData;

namespace {
// Types of password infobars to display.
enum class PasswordInfoBarType { SAVE, UPDATE };

// Types of password suggestion in the keyboard accessory. Used for metrics
// collection.
enum class PasswordSuggestionType {
  // Credentials are listed.
  CREDENTIALS = 0,
  // "Show All" is listed.
  SHOW_ALL = 1,
  // "Suggest Password" is listed.
  SUGGESTED = 2,
  COUNT
};

// Password is considered not generated when user edits it below 4 characters.
constexpr int kMinimumLengthForEditedPassword = 4;

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

@interface PasswordController ()<PasswordSuggestionHelperDelegate>

// View controller for auto sign-in notification, owned by this
// PasswordController.
@property(nonatomic, strong)
    NotifyUserAutoSigninViewController* notifyAutoSigninViewController;

// Helper contains common password form processing logic.
@property(nonatomic, readonly) PasswordFormHelper* formHelper;

// Helper contains common password suggestion logic.
@property(nonatomic, readonly) PasswordSuggestionHelper* suggestionHelper;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// Tracks if current password is generated.
@property(nonatomic, assign) BOOL isPasswordGenerated;

// Tracks field when current password was generated.
@property(nonatomic, copy) NSString* passwordGeneratedIdentifier;

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

@implementation PasswordController {
  std::unique_ptr<PasswordManager> _passwordManager;
  std::unique_ptr<PasswordGenerationFrameHelper> _passwordGenerationHelper;
  std::unique_ptr<PasswordManagerClient> _passwordManagerClient;
  std::unique_ptr<PasswordManagerDriver> _passwordManagerDriver;
  std::unique_ptr<CredentialManager> _credentialManager;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Timer for hiding "Signing in as ..." notification.
  base::OneShotTimer _notifyAutoSigninTimer;

  // User credential waiting to be displayed in autosign-in snackbar, once tab
  // becomes active.
  std::unique_ptr<autofill::PasswordForm> _pendingAutoSigninPasswordForm;

  // Form data for password generation on this page.
  std::map<base::string16, NewPasswordFormGenerationData> _formGenerationData;
}

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
    _formHelper =
        [[PasswordFormHelper alloc] initWithWebState:webState delegate:self];
    _suggestionHelper =
        [[PasswordSuggestionHelper alloc] initWithDelegate:self];
    if (passwordManagerClient)
      _passwordManagerClient = std::move(passwordManagerClient);
    else
      _passwordManagerClient.reset(new IOSChromePasswordManagerClient(self));
    _passwordManager.reset(new PasswordManager(_passwordManagerClient.get()));
    _passwordManagerDriver.reset(new IOSChromePasswordManagerDriver(self));

    if (features::IsAutomaticPasswordGenerationEnabled() &&
        !_passwordManagerClient->IsIncognito()) {
      _passwordGenerationHelper.reset(new PasswordGenerationFrameHelper(
          _passwordManagerClient.get(), _passwordManagerDriver.get()));
    }

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

- (ukm::SourceId)ukmSourceId {
  return _webState ? ukm::GetSourceIdForWebStateDocument(_webState)
                   : ukm::kInvalidSourceId;
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
  [self.formHelper findAndFillPasswordFormsWithUserName:username
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
  [self.suggestionHelper resetForNewPage];

  // Retrieve the identity of the page. In case the page might be malicous,
  // returns early.
  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webState, &pageURL))
    return;

  if (!web::UrlHasWebScheme(pageURL))
    return;

  // Notify the password manager that the page loaded so it can clear its own
  // per-page state.
  self.passwordManager->DidNavigateMainFrame(/*form_may_be_submitted=*/true);

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
  _formGenerationData.clear();
  _isPasswordGenerated = NO;
}

#pragma mark - FormSuggestionProvider

- (id<FormSuggestionProvider>)suggestionProvider {
  return self;
}

- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                           fieldIdentifier:(NSString*)fieldIdentifier
                                 fieldType:(NSString*)fieldType
                                      type:(NSString*)type
                                typedValue:(NSString*)typedValue
                                   frameID:(NSString*)frameID
                               isMainFrame:(BOOL)isMainFrame
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  [self.suggestionHelper
      checkIfSuggestionsAvailableForForm:formName
                         fieldIdentifier:fieldIdentifier
                               fieldType:fieldType
                                    type:type
                                 frameID:frameID
                             isMainFrame:isMainFrame
                                webState:webState
                       completionHandler:^(BOOL suggestionsAvailable) {
                         // Always display "Show All..." for password fields.
                         completion([fieldType isEqualToString:@"password"] ||
                                    suggestionsAvailable);
                       }];

  if (self.isPasswordGenerated &&
      [fieldIdentifier isEqualToString:self.passwordGeneratedIdentifier]) {
    // On other platforms, when the user clicks on generation field, we show
    // password in clear text. And the user has the possibility to edit it. On
    // iOS, it's harder to do (it's probably bad idea to change field type from
    // password to text). The decision was to give everything to the automatic
    // flow and avoid the manual flow, for a cleaner and simpler UI.
    if (typedValue.length < kMinimumLengthForEditedPassword) {
      self.isPasswordGenerated = NO;
      self.passwordGeneratedIdentifier = nil;
      self.passwordManager->OnPasswordNoLongerGenerated(
          self.passwordManagerDriver);
    } else {
      // Inject updated value to possibly update confirmation field.
      [self injectGeneratedPasswordForFormName:formName
                             generatedPassword:typedValue
                             completionHandler:nil];
    }
  }

  if (self.isPasswordGenerated) {
    // Always update, in case, for example, that username has been edited.
    self.passwordManager->UpdateGeneratedPasswordOnUserInput(
        SysNSStringToUTF16(formName), SysNSStringToUTF16(fieldIdentifier),
        SysNSStringToUTF16(typedValue));
  }
}

- (void)retrieveSuggestionsForForm:(NSString*)formName
                   fieldIdentifier:(NSString*)fieldIdentifier
                         fieldType:(NSString*)fieldType
                              type:(NSString*)type
                        typedValue:(NSString*)typedValue
                           frameID:(NSString*)frameID
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  DCHECK(GetPageURLAndCheckTrustLevel(webState, nullptr));
  NSArray<FormSuggestion*>* rawSuggestions =
      [self.suggestionHelper retrieveSuggestionsWithFormName:formName
                                             fieldIdentifier:fieldIdentifier
                                                   fieldType:fieldType];

  NSMutableArray<FormSuggestion*>* suggestions = [NSMutableArray array];
  for (FormSuggestion* rawSuggestion in rawSuggestions) {
    [suggestions
        addObject:[FormSuggestion
                      suggestionWithValue:
                          [rawSuggestion.value
                              stringByAppendingString:kSuggestionSuffix]
                       displayDescription:rawSuggestion.displayDescription
                                     icon:nil
                               identifier:0]];
  }
  if (suggestions.count) {
    LogSuggestionShown(PasswordSuggestionType::CREDENTIALS);
  }

  if ([self canGeneratePasswordForForm:formName
                       fieldIdentifier:fieldIdentifier
                             fieldType:fieldType]) {
    // Add "Suggest Password...".
    NSString* suggestPassword = GetNSString(IDS_IOS_SUGGEST_PASSWORD);
    [suggestions
        addObject:
            [FormSuggestion
                suggestionWithValue:suggestPassword
                 displayDescription:nil
                               icon:nil
                         identifier:autofill::
                                        POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY]];
    LogSuggestionShown(PasswordSuggestionType::SUGGESTED);
  }

  // Once Manual Fallback is enabled the access to settings will exist as an
  // option in the new passwords UI.
  if (!autofill::features::IsPasswordManualFallbackEnabled()) {
    // Add "Show all".
    NSString* showAll = GetNSString(IDS_IOS_SHOW_ALL_PASSWORDS);
    [suggestions
        addObject:
            [FormSuggestion
                suggestionWithValue:showAll
                 displayDescription:nil
                               icon:nil
                         identifier:
                             autofill::
                                 POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY]];
    LogSuggestionShown(PasswordSuggestionType::SHOW_ALL);
  }

  completion([suggestions copy], self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                       form:(NSString*)formName
            fieldIdentifier:(NSString*)fieldIdentifier
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  switch (suggestion.identifier) {
    case autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY: {
      // Navigate to the settings list.
      [self.delegate displaySavedPasswordList];
      completion();
      LogSuggestionClicked(PasswordSuggestionType::SHOW_ALL);
      return;
    }
    case autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY: {
      [self generatePasswordForFormName:formName
                      completionHandler:^(BOOL injected) {
                        if (injected)
                          completion();
                      }];
      LogSuggestionClicked(PasswordSuggestionType::SUGGESTED);
      return;
    }
    default: {
      LogSuggestionClicked(PasswordSuggestionType::CREDENTIALS);
      DCHECK([suggestion.value hasSuffix:kSuggestionSuffix]);
      NSString* username = [suggestion.value
          substringToIndex:suggestion.value.length - kSuggestionSuffix.length];
      std::unique_ptr<password_manager::FillData> fillData =
          [self.suggestionHelper getFillDataForUsername:username];

      if (!fillData) {
        completion();
        return;
      }

      [self.formHelper fillPasswordFormWithFillData:*fillData
                                  completionHandler:^(BOOL success) {
                                    completion();
                                  }];
      break;
    }
  }
}

#pragma mark - PasswordManagerClientDelegate

- (web::WebState*)webState {
  return _webState;
}

- (ios::ChromeBrowserState*)browserState {
  return _webState ? ios::ChromeBrowserState::FromBrowserState(
                         _webState->GetBrowserState())
                   : nullptr;
}

- (PasswordManager*)passwordManager {
  return _passwordManager.get();
}

- (const GURL&)lastCommittedURL {
  return self.formHelper.lastCommittedURL;
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
  self.notifyAutoSigninViewController =
      [[NotifyUserAutoSigninViewController alloc]
          initWithUsername:SysUTF16ToNSString(formSignedIn->username_value)
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
  [self.suggestionHelper processWithPasswordFormFillData:formData];
  [self.formHelper fillPasswordForm:formData
                  completionHandler:completionHandler];
}

- (void)onNoSavedCredentials {
  [self.suggestionHelper processWithNoSavedCredentials];
}

- (PasswordGenerationFrameHelper*)passwordGenerationHelper {
  return _passwordGenerationHelper.get();
}

- (void)formEligibleForGenerationFound:
    (const NewPasswordFormGenerationData&)form {
  _formGenerationData[form.form_name] = form;
}

#pragma mark - PasswordFormHelperDelegate

- (void)formHelper:(PasswordFormHelper*)formHelper
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

#pragma mark - PasswordSuggestionHelperDelegate

- (void)suggestionHelperShouldTriggerFormExtraction:
    (PasswordSuggestionHelper*)suggestionHelper {
  [self findPasswordFormsAndSendThemToPasswordStore];
}

#pragma mark - Private methods

- (void)didFinishPasswordFormExtraction:
    (const std::vector<autofill::PasswordForm>&)forms {
  // Do nothing if |self| has been detached.
  if (!self.passwordManager)
    return;

  if (!forms.empty()) {
    [self.suggestionHelper updateStateOnPasswordFormExtracted];

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
  [self.formHelper findPasswordFormsWithCompletionHandler:^(
                       const std::vector<autofill::PasswordForm>& forms) {
    [weakSelf didFinishPasswordFormExtraction:forms];
  }];
}

- (void)showInfoBarForForm:(std::unique_ptr<PasswordFormManagerForUI>)form
               infoBarType:(PasswordInfoBarType)type {
  if (!_webState)
    return;

  bool isSyncUser = false;
  if (self.browserState) {
    syncer::SyncService* sync_service =
        ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
    isSyncUser = password_bubble_experiment::IsSmartLockUser(sync_service);
  }
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(_webState);

  switch (type) {
    case PasswordInfoBarType::SAVE:
      IOSChromeSavePasswordInfoBarDelegate::Create(
          isSyncUser, infoBarManager, std::move(form), self.dispatcher);
      break;

    case PasswordInfoBarType::UPDATE:
      IOSChromeUpdatePasswordInfoBarDelegate::Create(
          isSyncUser, infoBarManager, std::move(form), self.baseViewController,
          self.dispatcher);
      break;
  }
}

- (void)hideAutosigninNotification {
  [self.notifyAutoSigninViewController willMoveToParentViewController:nil];
  [self.notifyAutoSigninViewController.view removeFromSuperview];
  [self.notifyAutoSigninViewController removeFromParentViewController];
  self.notifyAutoSigninViewController = nil;
}

- (BOOL)canGeneratePasswordForForm:(NSString*)formName
                   fieldIdentifier:(NSString*)fieldIdentifier
                         fieldType:(NSString*)fieldType {
  if (!features::IsAutomaticPasswordGenerationEnabled() ||
      _passwordManagerClient->IsIncognito() ||
      !_passwordManagerDriver->GetPasswordGenerationHelper()
           ->IsGenerationEnabled(
               /*log_debug_data*/ true))
    return NO;
  if (![fieldType isEqualToString:@"password"])
    return NO;
  const NewPasswordFormGenerationData* generation_data =
      [self getFormForGenerationFromFormName:formName];
  if (!generation_data)
    return NO;

  NSString* newPasswordIdentifier =
      SysUTF16ToNSString(generation_data->new_password_element);
  if ([fieldIdentifier isEqualToString:newPasswordIdentifier])
    return YES;

  // Don't show password generation if the field is 'confirm password'.
  return NO;
}

- (const NewPasswordFormGenerationData*)getFormForGenerationFromFormName:
    (NSString*)formName {
  const base::string16 name = SysNSStringToUTF16(formName);
  if (_formGenerationData.find(name) != _formGenerationData.end()) {
    return &_formGenerationData[name];
  }
  return nullptr;
}

- (void)generatePasswordForFormName:(NSString*)formName
                  completionHandler:(void (^)(BOOL))completionHandler {
  if (![self getFormForGenerationFromFormName:formName])
    return;

  // TODO(crbug.com/886583): form_signature, field_signature, max_length and
  // spec_priority in PGM::GeneratePassword are being refactored, passing 0 for
  // now to get a generic random password.
  base::string16 generatedPassword =
      _passwordGenerationHelper->GeneratePassword([self lastCommittedURL], 0, 0,
                                                  0, nullptr);

  NSString* title = GetNSStringF(IDS_IOS_SUGGESTED_PASSWORD, generatedPassword);
  NSString* message = GetNSString(IDS_IOS_SUGGESTED_PASSWORD_HINT);

  // TODO(crbug.com/886583): add eg tests
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                           title:title
                         message:message
                            rect:self.baseViewController.view.frame
                            view:self.baseViewController.view];
  self.actionSheetCoordinator.popoverArrowDirection = 0;
  self.actionSheetCoordinator.alertStyle =
      IsIPadIdiom() ? UIAlertControllerStyleAlert
                    : UIAlertControllerStyleActionSheet;

  NSString* nsPassword = SysUTF16ToNSString(generatedPassword);

  __weak PasswordController* weakSelf = self;
  [self.actionSheetCoordinator
      addItemWithTitle:GetNSString(IDS_IOS_USE_SUGGESTED_PASSWORD)
                action:^{
                  [weakSelf
                      injectGeneratedPasswordForFormName:formName
                                       generatedPassword:nsPassword
                                       completionHandler:completionHandler];
                }
                 style:UIAlertActionStyleDefault];

  [self.actionSheetCoordinator addItemWithTitle:GetNSString(IDS_CANCEL)
                                         action:^{
                                           if (completionHandler)
                                             completionHandler(NO);
                                         }
                                          style:UIAlertActionStyleCancel];

  [self.actionSheetCoordinator start];
}

- (void)injectGeneratedPasswordForFormName:(NSString*)formName
                         generatedPassword:(NSString*)generatedPassword
                         completionHandler:(void (^)(BOOL))completionHandler {
  const autofill::NewPasswordFormGenerationData* generation_data =
      [self getFormForGenerationFromFormName:formName];
  if (!generation_data)
    return;
  NSString* newPasswordIdentifier =
      SysUTF16ToNSString(generation_data->new_password_element);
  NSString* confirmPasswordIdentifier =
      SysUTF16ToNSString(generation_data->confirmation_password_element);

  auto generatedPasswordInjected = ^(BOOL success) {
    auto passwordPresaved = ^(BOOL found, const autofill::FormData& form) {
      if (found) {
        self.passwordManager->PresaveGeneratedPassword(
            self.passwordManagerDriver, form,
            SysNSStringToUTF16(generatedPassword),
            SysNSStringToUTF16(newPasswordIdentifier));
      }
      // If the form isn't found, it disappeared between fillPasswordForm below
      // and here. There isn't much that can be done.
    };
    if (success) {
      [self.formHelper extractPasswordFormData:formName
                             completionHandler:passwordPresaved];
      self.isPasswordGenerated = YES;
      self.passwordGeneratedIdentifier = newPasswordIdentifier;
    }
    if (completionHandler)
      completionHandler(YES);
  };

  [self.formHelper fillPasswordForm:formName
              newPasswordIdentifier:newPasswordIdentifier
          confirmPasswordIdentifier:confirmPasswordIdentifier
                  generatedPassword:generatedPassword
                  completionHandler:generatedPasswordInjected];
}

@end
