// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_controller.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/passwords/account_select_fill_data.h"
#include "ios/chrome/browser/passwords/credential_manager.h"
#include "ios/chrome/browser/passwords/credential_manager_features.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/ios_chrome_update_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/js_password_manager.h"
#import "ios/chrome/browser/passwords/notify_auto_signin_view_controller.h"
#import "ios/chrome/browser/passwords/password_form_filler.h"
#import "ios/chrome/browser/ssl/insecure_input_tab_helper.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#include "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/web/public/origin_util.h"
#include "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::PasswordFormManager;
using password_manager::PasswordManager;
using password_manager::PasswordManagerClient;
using password_manager::PasswordManagerDriver;

namespace {
// Types of password infobars to display.
enum class PasswordInfoBarType { SAVE, UPDATE };

// Duration for notify user auto-sign in dialog being displayed.
constexpr int kNotifyAutoSigninDuration = 3;  // seconds

// Script command prefix for form changes. Possible command to be sent from
// injected JS is 'form.buttonClicked'.
constexpr char kCommandPrefix[] = "passwordForm";
}

@interface PasswordController ()

// This is set to YES as soon as the associated WebState is destroyed.
@property(readonly) BOOL isWebStateDestroyed;

// View controller for auto sign-in notification, owned by this
// PasswordController.
@property(nonatomic, strong)
    NotifyUserAutoSigninViewController* notifyAutoSigninViewController;

@end

@interface PasswordController ()<FormSuggestionProvider, PasswordFormFiller>

// Parses the |jsonString| which contatins the password forms found on a web
// page to populate the |forms| vector.
- (void)getPasswordForms:(std::vector<autofill::PasswordForm>*)forms
           fromFormsJSON:(NSString*)jsonString
                 pageURL:(const GURL&)pageURL;

// Processes the JSON string returned as a result of extracting the submitted
// form data and populates |form|. Returns YES on success. NO otherwise.
// |form| cannot be nil.
- (BOOL)getPasswordForm:(autofill::PasswordForm*)form
   fromPasswordFormJSON:(NSString*)jsonString
                pageURL:(const GURL&)pageURL;

// Informs the |passwordManager_| of the password forms (if any were present)
// that have been found on the page.
- (void)didFinishPasswordFormExtraction:
    (const std::vector<autofill::PasswordForm>&)forms;

// Autofills |username| and |password| into the form specified by |formData|,
// invoking |completionHandler| when finished with YES if successful and
// NO otherwise. |completionHandler| may be nil.
- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
            withUsername:(const base::string16&)username
                password:(const base::string16&)password
       completionHandler:(void (^)(BOOL))completionHandler;

// Autofills credentials into the page. Credentials and input fields are
// specified by |fillData|. Invoking |completionHandler| when finished with YES
// if successful and NO otherwise. |completionHandler| may be nil.
- (void)fillPasswordFormWithFillData:(const password_manager::FillData&)fillData
                   completionHandler:(void (^)(BOOL))completionHandler;

// Uses JavaScript to find password forms. Calls |completionHandler| with the
// extracted information used for matching and saving passwords. Calls
// |completionHandler| with an empty vector if no password forms are found.
// |completionHandler| cannot be nil.
- (void)findPasswordFormsWithCompletionHandler:
    (void (^)(const std::vector<autofill::PasswordForm>&))completionHandler;

// Finds all password forms in DOM and sends them to the password store for
// fetching stored credentials.
- (void)findPasswordFormsAndSendThemToPasswordStore;

// Finds the currently submitted password form and calls |completionHandler|
// with the populated data structure. |found| is YES if the current form was
// found successfully, NO otherwise. |completionHandler| cannot be nil.
- (void)extractSubmittedPasswordForm:(const std::string&)formName
                   completionHandler:
                       (void (^)(BOOL found,
                                 const autofill::PasswordForm& form))
                           completionHandler;

// Takes values from a JSON |dictionary| and populates the |form|.
// The |pageLocation| is the URL of the current page.
// Returns YES if the form was correctly populated, NO otherwise.
- (BOOL)getPasswordForm:(autofill::PasswordForm*)form
         fromDictionary:(const base::DictionaryValue*)dictionary
                pageURL:(const GURL&)pageLocation;

// Displays infobar for |form| with |type|. If |type| is UPDATE, the user
// is prompted to update the password. If |type| is SAVE, the user is prompted
// to save the password.
- (void)showInfoBarForForm:(std::unique_ptr<PasswordFormManager>)form
               infoBarType:(PasswordInfoBarType)type;

// Handler for injected JavaScript callbacks.
- (BOOL)handleScriptCommand:(const base::DictionaryValue&)JSONCommand;

@end

namespace {

// Constructs an array of FormSuggestions, each corresponding to a username/
// password pair in |AccountSelectFillData|, such that |typedValue| is a prefix
// of the username of each suggestion.
NSArray* BuildSuggestions(const AccountSelectFillData& fillData,
                          NSString* formName,
                          NSString* fieldName,
                          NSString* typedValue) {
  base::string16 form_name = base::SysNSStringToUTF16(formName);
  base::string16 field_name = base::SysNSStringToUTF16(fieldName);
  base::string16 typed_value = base::SysNSStringToUTF16(typedValue);

  NSMutableArray* suggestions = [NSMutableArray array];
  std::vector<password_manager::UsernameAndRealm> username_and_realms_ =
      fillData.RetrieveSuggestions(form_name, field_name, typed_value);
  if (username_and_realms_.empty())
    return suggestions;

  for (const auto& username_and_realm : username_and_realms_) {
    NSString* username = base::SysUTF16ToNSString(username_and_realm.username);
    NSString* origin = username_and_realm.realm.empty()
                           ? nil
                           : base::SysUTF8ToNSString(username_and_realm.realm);

    [suggestions addObject:[FormSuggestion suggestionWithValue:username
                                            displayDescription:origin
                                                          icon:nil
                                                    identifier:0]];
  }

  return suggestions;
}

// Removes URL components not essential for matching the URL to
// saved password databases entries.  The stripped components are:
// user, password, query and ref.
GURL stripURL(GURL& url) {
  url::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

// Serializes arguments into a JSON string that can be used by the JS side
// of PasswordController.
NSString* SerializeFillData(const GURL& origin,
                            const GURL& action,
                            const base::string16& username_element,
                            const base::string16& username_value,
                            const base::string16& password_element,
                            const base::string16& password_value) {
  base::DictionaryValue rootDict;
  rootDict.SetString("origin", origin.spec());
  rootDict.SetString("action", action.spec());

  auto fieldList = base::MakeUnique<base::ListValue>();

  auto usernameField = base::MakeUnique<base::DictionaryValue>();
  usernameField->SetString("name", username_element);
  usernameField->SetString("value", username_value);
  fieldList->Append(std::move(usernameField));

  auto passwordField = base::MakeUnique<base::DictionaryValue>();
  passwordField->SetString("name", password_element);
  passwordField->SetString("value", password_value);
  fieldList->Append(std::move(passwordField));

  rootDict.Set("fields", std::move(fieldList));

  std::string jsonString;
  base::JSONWriter::Write(rootDict, &jsonString);
  return base::SysUTF8ToNSString(jsonString);
}

// Serializes |formData| into a JSON string that can be used by the JS side
// of PasswordController.
NSString* SerializePasswordFormFillData(
    const autofill::PasswordFormFillData& formData) {
  return SerializeFillData(
      formData.origin, formData.action, formData.username_field.name,
      formData.username_field.value, formData.password_field.name,
      formData.password_field.value);
}

NSString* SerializeFillData(const FillData& fillData) {
  return SerializeFillData(fillData.origin, fillData.action,
                           fillData.username_element, fillData.username_value,
                           fillData.password_element, fillData.password_value);
}

// Returns true if the trust level for the current page URL of |web_state| is
// kAbsolute. If |page_url| is not null, fills it with the current page URL.
bool GetPageURLAndCheckTrustLevel(web::WebState* web_state, GURL* page_url) {
  auto trustLevel = web::URLVerificationTrustLevel::kNone;
  GURL dummy;
  if (!page_url)
    page_url = &dummy;
  *page_url = web_state->GetCurrentURL(&trustLevel);
  return trustLevel == web::URLVerificationTrustLevel::kAbsolute;
}

}  // namespace

@implementation PasswordController {
  std::unique_ptr<PasswordManager> passwordManager_;
  std::unique_ptr<PasswordManagerClient> passwordManagerClient_;
  std::unique_ptr<PasswordManagerDriver> passwordManagerDriver_;
  std::unique_ptr<CredentialManager> credentialManager_;

  __weak JsPasswordManager* passwordJsManager_;

  AccountSelectFillData fillData_;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* webState_;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> webStateObserverBridge_;

  // Timer for hiding "Signing in as ..." notification.
  base::OneShotTimer notifyAutoSigninTimer_;

  // True indicates that a request for credentials has been sent to the password
  // store.
  BOOL sentRequestToStore_;

  // User credential waiting to be displayed in autosign-in snackbar, once tab
  // becomes active.
  std::unique_ptr<autofill::PasswordForm> pendingAutoSigninPasswordForm_;
}

@synthesize isWebStateDestroyed = _isWebStateDestroyed;

@synthesize dispatcher = _dispatcher;

@synthesize delegate = _delegate;

@synthesize notifyAutoSigninViewController = _notifyAutoSigninViewController;

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
    webState_ = webState;
    if (passwordManagerClient)
      passwordManagerClient_ = std::move(passwordManagerClient);
    else
      passwordManagerClient_.reset(new IOSChromePasswordManagerClient(self));
    passwordManager_.reset(new PasswordManager(passwordManagerClient_.get()));
    passwordManagerDriver_.reset(new IOSChromePasswordManagerDriver(self));

    passwordJsManager_ = base::mac::ObjCCastStrict<JsPasswordManager>(
        [webState_->GetJSInjectionReceiver()
            instanceOfClass:[JsPasswordManager class]]);
    webStateObserverBridge_ =
        std::make_unique<web::WebStateObserverBridge>(self);
    webState_->AddObserver(webStateObserverBridge_.get());
    sentRequestToStore_ = NO;

    if (base::FeatureList::IsEnabled(features::kCredentialManager)) {
      credentialManager_ = base::MakeUnique<CredentialManager>(
          passwordManagerClient_.get(), webState_);
    }

    __weak PasswordController* weakSelf = self;
    auto callback = base::BindBlockArc(^bool(const base::DictionaryValue& JSON,
                                             const GURL& originURL,
                                             bool userIsInteracting) {
      // |originURL| and |isInteracting| aren't used.
      return [weakSelf handleScriptCommand:JSON];
    });
    webState_->AddScriptCommandCallback(callback, kCommandPrefix);
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  [self detach];

  if (webState_) {
    webState_->RemoveObserver(webStateObserverBridge_.get());
    webStateObserverBridge_.reset();
    webState_ = nullptr;
  }
}

- (ios::ChromeBrowserState*)browserState {
  return webState_ ? ios::ChromeBrowserState::FromBrowserState(
                         webState_->GetBrowserState())
                   : nullptr;
}

- (const GURL&)lastCommittedURL {
  return webState_ ? webState_->GetLastCommittedURL() : GURL::EmptyGURL();
}

- (void)detach {
  if (webState_) {
    webState_->RemoveScriptCommandCallback(kCommandPrefix);
    webState_->RemoveObserver(webStateObserverBridge_.get());
    webStateObserverBridge_.reset();
    webState_ = nullptr;
  }

  passwordManagerDriver_.reset();
  passwordManager_.reset();
  passwordManagerClient_.reset();
  credentialManager_.reset();
}

#pragma mark -
#pragma mark Properties

- (id<PasswordFormFiller>)passwordFormFiller {
  return self;
}

#pragma mark -
#pragma mark PasswordFormFiller

- (void)findAndFillPasswordForms:(NSString*)username
                        password:(NSString*)password
               completionHandler:(void (^)(BOOL))completionHandler {
  [self findPasswordFormsWithCompletionHandler:^(
            const std::vector<autofill::PasswordForm>& forms) {
    for (const auto& form : forms) {
      autofill::PasswordFormFillData formData;
      std::map<base::string16, const autofill::PasswordForm*> matches;
      // Initialize |matches| to satisfy the expectation from
      // InitPasswordFormFillData() that the preferred match (3rd parameter)
      // should be one of the |matches|.
      matches.insert(std::make_pair(form.username_value, &form));
      autofill::InitPasswordFormFillData(form, matches, &form, false, false,
                                         &formData);
      [self fillPasswordForm:formData
                withUsername:base::SysNSStringToUTF16(username)
                    password:base::SysNSStringToUTF16(password)
           completionHandler:completionHandler];
    }
  }];
}

#pragma mark -
#pragma mark CRWWebStateObserver

// If Tab was shown, and there is a pending PasswordForm, display autosign-in
// notification.
- (void)webStateWasShown:(web::WebState*)webState {
  DCHECK_EQ(webState_, webState);
  if (pendingAutoSigninPasswordForm_) {
    [self showAutosigninNotification:std::move(pendingAutoSigninPasswordForm_)];
    pendingAutoSigninPasswordForm_.reset();
  }
}

// If Tab was hidden, hide auto sign-in notification.
- (void)webStateWasHidden:(web::WebState*)webState {
  DCHECK_EQ(webState_, webState);
  [self hideAutosigninNotification];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(webState_, webState);
  // Clear per-page state.
  fillData_.Reset();
  sentRequestToStore_ = NO;

  // Retrieve the identity of the page. In case the page might be malicous,
  // returns early.
  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webState, &pageURL))
    return;

  if (!web::UrlHasWebScheme(pageURL))
    return;

  // Notify the password manager that the page loaded so it can clear its own
  // per-page state.
  passwordManager_->DidNavigateMainFrame();

  if (!webState->ContentIsHTML()) {
    // If the current page is not HTML, it does not contain any HTML forms.
    [self
        didFinishPasswordFormExtraction:std::vector<autofill::PasswordForm>()];
  }

  [self findPasswordFormsAndSendThemToPasswordStore];
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                     userInitiated:(BOOL)userInitiated {
  DCHECK_EQ(webState_, webState);
  __weak PasswordController* weakSelf = self;
  // This code is racing against the new page loading and will not get the
  // password form data if the page has changed. In most cases this code wins
  // the race.
  // TODO(crbug.com/418827): Fix this by passing in more data from the JS side.
  id completionHandler = ^(BOOL found, const autofill::PasswordForm& form) {
    PasswordController* strongSelf = weakSelf;
    if (strongSelf && ![strongSelf isWebStateDestroyed]) {
      strongSelf.passwordManager->OnPasswordFormSubmitted(
          strongSelf.passwordManagerDriver, form);
    }
  };
  [self extractSubmittedPasswordForm:formName
                   completionHandler:completionHandler];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(webState_, webState);
  _isWebStateDestroyed = YES;
  [self detach];
}

#pragma mark - Private methods.

- (void)findPasswordFormsWithCompletionHandler:
    (void (^)(const std::vector<autofill::PasswordForm>&))completionHandler {
  DCHECK(completionHandler);

  if (!webState_)
    return;

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webState_, &pageURL)) {
    return;
  }

  __weak PasswordController* weakSelf = self;
  [passwordJsManager_ findPasswordFormsWithCompletionHandler:^(
                          NSString* jsonString) {
    std::vector<autofill::PasswordForm> forms;
    [weakSelf getPasswordForms:&forms fromFormsJSON:jsonString pageURL:pageURL];
    completionHandler(forms);
  }];
}

- (void)findPasswordFormsAndSendThemToPasswordStore {
  // Read all password forms from the page and send them to the password
  // manager.
  __weak PasswordController* weakSelf = self;
  [self findPasswordFormsWithCompletionHandler:^(
            const std::vector<autofill::PasswordForm>& forms) {
    [weakSelf didFinishPasswordFormExtraction:forms];
  }];
}

- (void)getPasswordForms:(std::vector<autofill::PasswordForm>*)forms
           fromFormsJSON:(NSString*)JSONNSString
                 pageURL:(const GURL&)pageURL {
  DCHECK(forms);
  std::string JSONString = base::SysNSStringToUTF8(JSONNSString);
  if (JSONString.empty()) {
    VLOG(1) << "Error in password controller javascript.";
    return;
  }

  int errorCode = 0;
  std::string errorMessage;
  std::unique_ptr<base::Value> jsonData(base::JSONReader::ReadAndReturnError(
      JSONString, false, &errorCode, &errorMessage));
  if (errorCode || !jsonData || !jsonData->IsType(base::Value::Type::LIST)) {
    VLOG(1) << "JSON parse error " << errorMessage
            << " JSON string: " << JSONString;
    return;
  }

  const base::ListValue* formDataList;
  if (!jsonData->GetAsList(&formDataList))
    return;
  for (size_t i = 0; i != formDataList->GetSize(); ++i) {
    const base::DictionaryValue* formData;
    if (formDataList->GetDictionary(i, &formData)) {
      autofill::PasswordForm form;
      if ([self getPasswordForm:&form
                 fromDictionary:formData
                        pageURL:pageURL]) {
        forms->push_back(form);
      }
    }
  }
}

- (void)extractSubmittedPasswordForm:(const std::string&)formName
                   completionHandler:
                       (void (^)(BOOL found,
                                 const autofill::PasswordForm& form))
                           completionHandler {
  DCHECK(completionHandler);

  if (!webState_)
    return;

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webState_, &pageURL)) {
    completionHandler(NO, autofill::PasswordForm());
    return;
  }

  __weak PasswordController* weakSelf = self;
  id extractSubmittedFormCompletionHandler = ^(NSString* jsonString) {
    autofill::PasswordForm form;
    BOOL found = [weakSelf getPasswordForm:&form
                      fromPasswordFormJSON:jsonString
                                   pageURL:pageURL];
    completionHandler(found, form);
  };

  [passwordJsManager_ extractForm:base::SysUTF8ToNSString(formName)
                completionHandler:extractSubmittedFormCompletionHandler];
}

- (BOOL)getPasswordForm:(autofill::PasswordForm*)form
    fromPasswordFormJSON:(NSString*)JSONNSString
                 pageURL:(const GURL&)pageURL {
  DCHECK(form);
  // There is no identifiable password form on the page.
  if ([JSONNSString isEqualToString:@"noPasswordsFound"])
    return NO;

  int errorCode = 0;
  std::string errorMessage;
  std::string JSONString = base::SysNSStringToUTF8(JSONNSString);
  std::unique_ptr<const base::Value> JSONData(
      base::JSONReader::ReadAndReturnError(JSONString, false, &errorCode,
                                           &errorMessage));

  // If the the JSON string contains null, there is no identifiable password
  // form on the page.
  if (!errorCode && !JSONData) {
    return NO;
  }

  if (errorCode || !JSONData->IsType(base::Value::Type::DICTIONARY)) {
    VLOG(1) << "JSON parse error " << errorMessage
            << " JSON string: " << JSONString;
    return NO;
  }

  const base::DictionaryValue* passwordJsonData;
  return JSONData->GetAsDictionary(&passwordJsonData) &&
         [self getPasswordForm:form
                fromDictionary:passwordJsonData
                       pageURL:pageURL];
}

- (void)didFinishPasswordFormExtraction:
    (const std::vector<autofill::PasswordForm>&)forms {
  // Do nothing if |self| has been detached.
  if (!passwordManager_)
    return;

  if (!forms.empty()) {
    // Notify web_state about password forms, so that this can be taken into
    // account for the security state.
    if (webState_ && !web::IsOriginSecure(webState_->GetLastCommittedURL())) {
      InsecureInputTabHelper::GetOrCreateForWebState(webState_)
          ->DidShowPasswordFieldInInsecureContext();
    }

    sentRequestToStore_ = YES;
    // Invoke the password manager callback to autofill password forms
    // on the loaded page.
    passwordManager_->OnPasswordFormsParsed(passwordManagerDriver_.get(),
                                            forms);
  }
  // Invoke the password manager callback to check if password was
  // accepted or rejected. If accepted, infobar is presented. If
  // rejected, the provisionally saved password is deleted. On Chrome
  // w/ a renderer, it is the renderer who calls OnPasswordFormsParsed()
  // and OnPasswordFormsRendered(). Bling has to improvised a bit on the
  // ordering of these two calls.
  passwordManager_->OnPasswordFormsRendered(passwordManagerDriver_.get(), forms,
                                            true);
}

#pragma mark -
#pragma mark FormSuggestionProvider

- (id<FormSuggestionProvider>)suggestionProvider {
  return self;
}

- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                                     field:(NSString*)fieldName
                                 fieldType:(NSString*)fieldType
                                      type:(NSString*)type
                                typedValue:(NSString*)typedValue
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  if (!sentRequestToStore_ && [type isEqual:@"focus"]) {
    [self findPasswordFormsAndSendThemToPasswordStore];
    completion(NO);
    return;
  }
  if (fillData_.Empty() || !GetPageURLAndCheckTrustLevel(webState, nullptr)) {
    completion(NO);
    return;
  }

  // Suggestions are available for the username field of the password form.
  completion(fillData_.IsSuggestionsAvailable(
      base::SysNSStringToUTF16(formName), base::SysNSStringToUTF16(fieldName)));
}

- (void)retrieveSuggestionsForForm:(NSString*)formName
                             field:(NSString*)fieldName
                         fieldType:(NSString*)fieldType
                              type:(NSString*)type
                        typedValue:(NSString*)typedValue
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  DCHECK(GetPageURLAndCheckTrustLevel(webState, nullptr));
  if (fillData_.Empty()) {
    completion(@[], nil);
    return;
  }
  completion(BuildSuggestions(fillData_, formName, fieldName, typedValue),
             self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                   forField:(NSString*)fieldName
                       form:(NSString*)formName
          completionHandler:(SuggestionHandledCompletion)completion {
  const base::string16 username = base::SysNSStringToUTF16(suggestion.value);
  std::unique_ptr<FillData> fillData = fillData_.GetFillData(username);

  if (!fillData)
    completion();

  [self fillPasswordFormWithFillData:*fillData
                   completionHandler:^(BOOL success) {
                     completion();
                   }];
}

#pragma mark - PasswordManagerClientDelegate

- (void)showSavePasswordInfoBar:
    (std::unique_ptr<PasswordFormManager>)formToSave {
  [self showInfoBarForForm:std::move(formToSave)
               infoBarType:PasswordInfoBarType::SAVE];
}

- (void)showUpdatePasswordInfoBar:
    (std::unique_ptr<PasswordFormManager>)formToUpdate {
  [self showInfoBarForForm:std::move(formToUpdate)
               infoBarType:PasswordInfoBarType::UPDATE];
}

// Hides auto sign-in notification. Removes the view from superview and destroys
// the controller.
// TODO(crbug.com/435048): Animate disappearance.
- (void)hideAutosigninNotification {
  [self.notifyAutoSigninViewController willMoveToParentViewController:nil];
  [self.notifyAutoSigninViewController.view removeFromSuperview];
  [self.notifyAutoSigninViewController removeFromParentViewController];
  self.notifyAutoSigninViewController = nil;
}

// Shows auto sign-in notification and schedules hiding it after 3 seconds.
// TODO(crbug.com/435048): Animate appearance.
- (void)showAutosigninNotification:
    (std::unique_ptr<autofill::PasswordForm>)formSignedIn {
  if (!webState_)
    return;

  // If a notification is already being displayed, hides the old one, then shows
  // the new one.
  if (self.notifyAutoSigninViewController) {
    notifyAutoSigninTimer_.Stop();
    [self hideAutosigninNotification];
  }

  // Creates view controller then shows the subview.
  self.notifyAutoSigninViewController = [
      [NotifyUserAutoSigninViewController alloc]
      initWithUsername:base::SysUTF16ToNSString(formSignedIn->username_value)
               iconURL:formSignedIn->icon_url
         contextGetter:webState_->GetBrowserState()->GetRequestContext()];
  TabIdTabHelper* tabIdHelper = TabIdTabHelper::FromWebState(webState_);
  if (![_delegate displaySignInNotification:self.notifyAutoSigninViewController
                                  fromTabId:tabIdHelper->tab_id()]) {
    // The notification was not shown. Store the password form in
    // |pendingAutoSigninPasswordForm_| to show the notification later.
    pendingAutoSigninPasswordForm_ = std::move(formSignedIn);
    self.notifyAutoSigninViewController = nil;
    return;
  }

  // Hides notification after 3 seconds.
  __weak PasswordController* weakSelf = self;
  notifyAutoSigninTimer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kNotifyAutoSigninDuration),
      base::BindBlockArc(^{
        [weakSelf hideAutosigninNotification];
      }));
}

#pragma mark -
#pragma mark WebPasswordFormData Adaptation

- (BOOL)getPasswordForm:(autofill::PasswordForm*)form
         fromDictionary:(const base::DictionaryValue*)dictionary
                pageURL:(const GURL&)pageLocation {
  DCHECK(form);
  DCHECK(dictionary);
  DCHECK(pageLocation.is_valid());

  base::string16 formOrigin;
  if (!(dictionary->GetString("origin", &formOrigin)) ||
      (GURL(formOrigin).GetOrigin() != pageLocation.GetOrigin())) {
    return NO;
  }

  std::string origin = pageLocation.spec();
  GURL originUrl(origin);
  if (!originUrl.is_valid()) {
    return NO;
  }
  form->origin = stripURL(originUrl);
  url::Replacements<char> remove_path;
  remove_path.ClearPath();
  form->signon_realm = form->origin.ReplaceComponents(remove_path).spec();

  std::string action;
  dictionary->GetString("action", &action);
  form->action = GURL(action);

  if (!dictionary->GetString("usernameElement", &form->username_element) ||
      !dictionary->GetString("usernameValue", &form->username_value) ||
      !dictionary->GetString("name", &form->form_data.name)) {
    return NO;
  }

  const base::ListValue* passwordDataList;
  if (!dictionary->GetList("passwords", &passwordDataList))
    return NO;

  size_t passwordCount = passwordDataList->GetSize();
  if (passwordCount == 0)
    return NO;

  const base::DictionaryValue* passwordData;

  // The "passwords" list contains element/value pairs for
  // password inputs found in the form.  We recognize
  // up to three passwords in any given form and ignore the rest.
  const size_t kMaxPasswordsConsidered = 3;
  base::string16 elements[kMaxPasswordsConsidered];
  base::string16 values[kMaxPasswordsConsidered];

  for (size_t i = 0; i < std::min(passwordCount, kMaxPasswordsConsidered);
       ++i) {
    if (!passwordDataList->GetDictionary(i, &passwordData) ||
        !passwordData->GetString("element", &elements[i]) ||
        !passwordData->GetString("value", &values[i])) {
      return NO;
    }
  }

  // Determine which password is the main one, and which is
  // an old password (e.g on a "make new password" form), if any.
  // TODO(crbug.com/564578): Make this compatible with other platforms.
  switch (passwordCount) {
    case 1:
      form->password_element = elements[0];
      form->password_value = values[0];
      break;
    case 2: {
      if (!values[0].empty() && values[0] == values[1]) {
        // Treat two identical passwords as a single password new password, with
        // confirmation. This can be either be a sign-up form or a password
        // change form that does not ask for a new password.
        form->new_password_element = elements[0];
        form->new_password_value = values[0];
      } else {
        // Assume first is old password, second is new (no choice but to guess).
        form->password_element = elements[0];
        form->password_value = values[0];
        form->new_password_element = elements[1];
        form->new_password_value = values[1];
      }
      break;
      default:
        if (!values[0].empty() && values[0] == values[1] &&
            values[0] == values[2]) {
          // All three passwords the same? This does not make sense, do not
          // add the password element.
          break;
        } else if (values[0] == values[1]) {
          // First two the same and the third different implies that the old
          // password is the duplicated one.
          form->password_element = elements[0];
          form->password_value = values[0];
          form->new_password_element = elements[2];
          form->new_password_value = values[2];
        } else if (values[1] == values[2]) {
          // Two the same and one different -> new password is the duplicated
          // one.
          form->password_element = elements[0];
          form->password_value = values[0];
          form->new_password_element = elements[1];
          form->new_password_value = values[1];
        } else {
          // Three different passwords, or first and last match with middle
          // different. No idea which is which, so no luck.
        }
        break;
    }
  }

  return YES;
}

- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
            withUsername:(const base::string16&)username
                password:(const base::string16&)password
       completionHandler:(void (^)(BOOL))completionHandler {
  if (formData.origin.GetOrigin() != self.lastCommittedURL.GetOrigin()) {
    if (completionHandler)
      completionHandler(NO);
    return;
  }

  // Send JSON over to the web view.
  [passwordJsManager_ fillPasswordForm:SerializePasswordFormFillData(formData)
                          withUsername:base::SysUTF16ToNSString(username)
                              password:base::SysUTF16ToNSString(password)
                     completionHandler:^(BOOL result) {
                       if (completionHandler)
                         completionHandler(result);
                     }];
}

- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
       completionHandler:(void (^)(BOOL))completionHandler {
  fillData_.Add(formData);

  // Don't fill immediately if waiting for the user to type a username.
  if (formData.wait_for_username) {
    if (completionHandler)
      completionHandler(NO);
    return;
  }

  [self fillPasswordForm:formData
            withUsername:formData.username_field.value
                password:formData.password_field.value
       completionHandler:completionHandler];
}

- (void)fillPasswordFormWithFillData:(const password_manager::FillData&)fillData
                   completionHandler:(void (^)(BOOL))completionHandler {
  // Send JSON over to the web view.
  [passwordJsManager_
       fillPasswordForm:SerializeFillData(fillData)
           withUsername:base::SysUTF16ToNSString(fillData.username_value)
               password:base::SysUTF16ToNSString(fillData.password_value)
      completionHandler:^(BOOL result) {
        if (completionHandler)
          completionHandler(result);
      }];
}

- (BOOL)handleScriptCommand:(const base::DictionaryValue&)JSONCommand {
  std::string command;
  if (!JSONCommand.GetString("command", &command))
    return NO;

  if (command != "passwordForm.submitButtonClick")
    return NO;

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webState_, &pageURL))
    return NO;
  autofill::PasswordForm form;
  BOOL formParsedFromJSON =
      [self getPasswordForm:&form fromDictionary:&JSONCommand pageURL:pageURL];
  if (formParsedFromJSON && ![self isWebStateDestroyed]) {
    self.passwordManager->OnPasswordFormSubmitted(self.passwordManagerDriver,
                                                  form);
    return YES;
  }

  return NO;
}

- (PasswordManagerClient*)passwordManagerClient {
  return passwordManagerClient_.get();
}

- (PasswordManagerDriver*)passwordManagerDriver {
  return passwordManagerDriver_.get();
}

- (PasswordManager*)passwordManager {
  return passwordManager_.get();
}

- (JsPasswordManager*)passwordJsManager {
  return passwordJsManager_;
}

#pragma mark - Private methods

- (void)showInfoBarForForm:(std::unique_ptr<PasswordFormManager>)form
               infoBarType:(PasswordInfoBarType)type {
  if (!webState_)
    return;

  bool isSmartLockBrandingEnabled = false;
  if (self.browserState) {
    syncer::SyncService* sync_service =
        IOSChromeProfileSyncServiceFactory::GetForBrowserState(
            self.browserState);
    isSmartLockBrandingEnabled =
        password_bubble_experiment::IsSmartLockUser(sync_service);
  }
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState_);

  switch (type) {
    case PasswordInfoBarType::SAVE:
      IOSChromeSavePasswordInfoBarDelegate::Create(
          isSmartLockBrandingEnabled, infoBarManager, std::move(form),
          self.dispatcher);
      break;

    case PasswordInfoBarType::UPDATE:
      IOSChromeUpdatePasswordInfoBarDelegate::Create(
          isSmartLockBrandingEnabled, infoBarManager, std::move(form),
          self.dispatcher);
      break;
  }
}

@end
