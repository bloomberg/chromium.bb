// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_controller.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#import "base/ios/weak_nsobject.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/sync_driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/js_password_manager.h"
#import "ios/chrome/browser/passwords/password_generation_agent.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

using password_manager::PasswordFormManager;
using password_manager::PasswordGenerationManager;
using password_manager::PasswordManager;
using password_manager::PasswordManagerClient;
using password_manager::PasswordManagerDriver;

@interface PasswordController ()

// This is set to YES as soon as the associated WebState is destroyed.
@property(readonly) BOOL isWebStateDestroyed;

@end

@interface PasswordController ()<FormSuggestionProvider>

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

// Uses JavaScript to find password forms. Calls |completionHandler| with the
// extracted information used for matching and saving passwords. Calls
// |completionHandler| with an empty vector if no password forms are found.
// |completionHandler| cannot be nil.
- (void)findPasswordFormsWithCompletionHandler:
    (void (^)(const std::vector<autofill::PasswordForm>&))completionHandler;

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

@end

namespace {

// Constructs an array of FormSuggestions, each corresponding to a username/
// password pair in |formFillData|, such that |prefix| is a prefix of the
// username of each suggestion.
NSArray* BuildSuggestions(const autofill::PasswordFormFillData& formFillData,
                          NSString* prefix) {
  NSMutableArray* suggestions = [NSMutableArray array];

  // Add the default credentials.
  NSString* defaultUsername =
      base::SysUTF16ToNSString(formFillData.username_field.value);
  if ([prefix length] == 0 ||
      [defaultUsername rangeOfString:prefix].location == 0) {
    NSString* origin =
        formFillData.preferred_realm.empty()
            ? nil
            : base::SysUTF8ToNSString(formFillData.preferred_realm);
    [suggestions addObject:[FormSuggestion suggestionWithValue:defaultUsername
                                            displayDescription:origin
                                                          icon:nil
                                                    identifier:0]];
  }

  // Add the additional credentials.
  for (const auto& it : formFillData.additional_logins) {
    NSString* additionalUsername = base::SysUTF16ToNSString(it.first);
    NSString* additionalOrigin = it.second.realm.empty()
                                     ? nil
                                     : base::SysUTF8ToNSString(it.second.realm);
    if ([prefix length] == 0 ||
        [additionalUsername rangeOfString:prefix].location == 0) {
      [suggestions
          addObject:[FormSuggestion suggestionWithValue:additionalUsername
                                     displayDescription:additionalOrigin
                                                   icon:nil
                                             identifier:0]];
    }
  }

  return suggestions;
}

// Looks for a credential pair in |formData| for with |username|. If such a pair
// exists, returns true and |matchingPassword|; returns false otherwise.
bool FindMatchingUsername(const autofill::PasswordFormFillData& formData,
                          const base::string16& username,
                          base::string16* matchingPassword) {
  base::string16 defaultUsername = formData.username_field.value;
  if (defaultUsername == username) {
    *matchingPassword = formData.password_field.value;
    return true;
  }

  // Check whether the user has finished typing an alternate username.
  for (const auto& it : formData.additional_logins) {
    if (it.first == username) {
      *matchingPassword = it.second.password;
      return true;
    }
  }

  return false;
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

// Serializes |formData| into a JSON string that can be used by the JS side
// of PasswordController.
NSString* SerializePasswordFormFillData(
    const autofill::PasswordFormFillData& formData) {
  // Repackage PasswordFormFillData as a JSON object.
  base::DictionaryValue rootDict;

  // The normalized URL of the web page.
  rootDict.SetString("origin", formData.origin.spec());

  // The normalized URL from the "action" attribute of the <form> tag.
  rootDict.SetString("action", formData.action.spec());

  // Input elements in the form. The list does not necessarily contain
  // all elements from the form, but all elements listed here are required
  // to identify the right form to fill.
  auto fieldList = base::WrapUnique(new base::ListValue());

  auto usernameField = base::WrapUnique(new base::DictionaryValue());
  usernameField->SetString("name", formData.username_field.name);
  usernameField->SetString("value", formData.username_field.value);
  fieldList->Append(usernameField.release());

  auto passwordField = base::WrapUnique(new base::DictionaryValue());
  passwordField->SetString("name", formData.password_field.name);
  passwordField->SetString("value", formData.password_field.value);
  fieldList->Append(passwordField.release());

  rootDict.Set("fields", fieldList.release());

  std::string jsonString;
  base::JSONWriter::Write(rootDict, &jsonString);
  return base::SysUTF8ToNSString(jsonString);
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
  std::unique_ptr<PasswordGenerationManager> passwordGenerationManager_;
  std::unique_ptr<PasswordManagerClient> passwordManagerClient_;
  std::unique_ptr<PasswordManagerDriver> passwordManagerDriver_;
  base::scoped_nsobject<PasswordGenerationAgent> passwordGenerationAgent_;

  JsPasswordManager* passwordJsManager_;  // weak

  // The pending form data.
  std::unique_ptr<autofill::PasswordFormFillData> formData_;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> webStateObserverBridge_;
}

@synthesize isWebStateDestroyed = isWebStateDestroyed_;

- (instancetype)initWithWebState:(web::WebState*)webState
             passwordsUiDelegate:(id<PasswordsUiDelegate>)UIDelegate {
  self = [self initWithWebState:webState
            passwordsUiDelegate:UIDelegate
                         client:nullptr];
  return self;
}

- (instancetype)initWithWebState:(web::WebState*)webState
             passwordsUiDelegate:(id<PasswordsUiDelegate>)UIDelegate
                          client:(std::unique_ptr<PasswordManagerClient>)
                                     passwordManagerClient {
  DCHECK(webState);
  self = [super init];
  if (self) {
    webStateObserverBridge_.reset(
        new web::WebStateObserverBridge(webState, self));
    if (passwordManagerClient)
      passwordManagerClient_ = std::move(passwordManagerClient);
    else
      passwordManagerClient_.reset(new IOSChromePasswordManagerClient(self));
    passwordManager_.reset(new PasswordManager(passwordManagerClient_.get()));
    passwordManagerDriver_.reset(new IOSChromePasswordManagerDriver(self));
    if (experimental_flags::IsPasswordGenerationEnabled() &&
        !passwordManagerClient_->IsOffTheRecord()) {
      passwordGenerationManager_.reset(new PasswordGenerationManager(
          passwordManagerClient_.get(), passwordManagerDriver_.get()));
      passwordGenerationAgent_.reset([[PasswordGenerationAgent alloc]
               initWithWebState:webState
                passwordManager:passwordManager_.get()
          passwordManagerDriver:passwordManagerDriver_.get()
            passwordsUiDelegate:UIDelegate]);
    }

    passwordJsManager_ = base::mac::ObjCCastStrict<JsPasswordManager>(
        [webState->GetJSInjectionReceiver()
            instanceOfClass:[JsPasswordManager class]]);
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  [self detach];
  [super dealloc];
}

- (ios::ChromeBrowserState*)browserState {
  return webStateObserverBridge_ && webStateObserverBridge_->web_state()
             ? ios::ChromeBrowserState::FromBrowserState(
                   webStateObserverBridge_->web_state()->GetBrowserState())
             : nullptr;
}

- (const GURL&)lastCommittedURL {
  if (!webStateObserverBridge_ || !webStateObserverBridge_->web_state())
    return GURL::EmptyGURL();
  return webStateObserverBridge_->web_state()->GetLastCommittedURL();
}

- (void)detach {
  webStateObserverBridge_.reset();
  passwordGenerationAgent_.reset();
  passwordGenerationManager_.reset();
  passwordManagerDriver_.reset();
  passwordManager_.reset();
  passwordManagerClient_.reset();
}

- (void)findAndFillPasswordForms:(NSString*)username
                        password:(NSString*)password
               completionHandler:(void (^)(BOOL))completionHandler {
  [self findPasswordFormsWithCompletionHandler:^(
            const std::vector<autofill::PasswordForm>& forms) {
    for (const auto& form : forms) {
      autofill::PasswordFormFillData formData;
      autofill::PasswordFormMap matches;
      // Initialize |matches| to satisfy the expectation from
      // InitPasswordFormFillData() that the preferred match (3rd parameter)
      // should be one of the |matches|.
      auto scoped_form = base::WrapUnique(new autofill::PasswordForm(form));
      matches.insert(
          std::make_pair(form.username_value, std::move(scoped_form)));
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

- (void)webStateDidLoadPage:(web::WebState*)webState {
  // Clear per-page state.
  formData_.reset();

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

  // Read all password forms from the page and send them to the password
  // manager.
  base::WeakNSObject<PasswordController> weakSelf(self);
  [self findPasswordFormsWithCompletionHandler:^(
            const std::vector<autofill::PasswordForm>& forms) {
    [weakSelf didFinishPasswordFormExtraction:forms];
  }];
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                     userInitiated:(BOOL)userInitiated {
  base::WeakNSObject<PasswordController> weakSelf(self);
  // This code is racing against the new page loading and will not get the
  // password form data if the page has changed. In most cases this code wins
  // the race.
  // TODO(crbug.com/418827): Fix this by passing in more data from the JS side.
  id completionHandler = ^(BOOL found, const autofill::PasswordForm& form) {
    if (weakSelf && ![weakSelf isWebStateDestroyed]) {
      weakSelf.get()->passwordManager_->OnPasswordFormSubmitted(
          weakSelf.get()->passwordManagerDriver_.get(), form);
    }
  };
  [self extractSubmittedPasswordForm:formName
                   completionHandler:completionHandler];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  isWebStateDestroyed_ = YES;
  [self detach];
}

- (void)findPasswordFormsWithCompletionHandler:
    (void (^)(const std::vector<autofill::PasswordForm>&))completionHandler {
  DCHECK(completionHandler);

  if (!webStateObserverBridge_ || !webStateObserverBridge_->web_state())
    return;

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webStateObserverBridge_->web_state(),
                                    &pageURL)) {
    return;
  }

  base::WeakNSObject<PasswordController> weakSelf(self);
  [passwordJsManager_ findPasswordFormsWithCompletionHandler:^(
                          NSString* jsonString) {
    std::vector<autofill::PasswordForm> forms;
    [weakSelf getPasswordForms:&forms fromFormsJSON:jsonString pageURL:pageURL];
    completionHandler(forms);
  }];
}

- (void)getPasswordForms:(std::vector<autofill::PasswordForm>*)forms
           fromFormsJSON:(NSString*)jsonString
                 pageURL:(const GURL&)pageURL {
  DCHECK(forms);
  if (![jsonString length]) {
    VLOG(1) << "Error in password controller javascript.";
    return;
  }

  int errorCode = 0;
  std::string errorMessage;
  std::unique_ptr<base::Value> jsonData(base::JSONReader::ReadAndReturnError(
      std::string([jsonString UTF8String]), false, &errorCode, &errorMessage));
  if (errorCode || !jsonData || !jsonData->IsType(base::Value::TYPE_LIST)) {
    VLOG(1) << "JSON parse error " << errorMessage
            << " JSON string: " << [jsonString UTF8String];
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

  if (!webStateObserverBridge_ || !webStateObserverBridge_->web_state())
    return;

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webStateObserverBridge_->web_state(),
                                    &pageURL)) {
    completionHandler(NO, autofill::PasswordForm());
    return;
  }

  base::WeakNSObject<PasswordController> weakSelf(self);
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
   fromPasswordFormJSON:(NSString*)jsonString
                pageURL:(const GURL&)pageURL {
  DCHECK(form);
  // There is no identifiable password form on the page.
  if ([jsonString isEqualToString:@"noPasswordsFound"])
    return NO;

  int errorCode = 0;
  std::string errorMessage;
  std::unique_ptr<const base::Value> jsonData(
      base::JSONReader::ReadAndReturnError(std::string([jsonString UTF8String]),
                                           false, &errorCode, &errorMessage));

  // If the the JSON string contains null, there is no identifiable password
  // form on the page.
  if (!errorCode && !jsonData) {
    return NO;
  }

  if (errorCode || !jsonData->IsType(base::Value::TYPE_DICTIONARY)) {
    VLOG(1) << "JSON parse error " << errorMessage
            << " JSON string: " << [jsonString UTF8String];
    return NO;
  }

  const base::DictionaryValue* passwordJsonData;
  return jsonData->GetAsDictionary(&passwordJsonData) &&
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
    // Invoke the password manager callback to autofill password forms
    // on the loaded page.
    passwordManager_->OnPasswordFormsParsed(passwordManagerDriver_.get(),
                                            forms);

    // Pass the forms to PasswordGenerationAgent to look for account creation
    // forms with local heuristics.
    [passwordGenerationAgent_ processParsedPasswordForms:forms];
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

- (id<FormInputAccessoryViewProvider>)accessoryViewProvider {
  return [passwordGenerationAgent_ accessoryViewProvider];
}

#pragma mark -
#pragma mark FormSuggestionProvider

- (id<FormSuggestionProvider>)suggestionProvider {
  return self;
}

- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                                     field:(NSString*)fieldName
                                      type:(NSString*)type
                                typedValue:(NSString*)typedValue
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  if (!formData_ || !GetPageURLAndCheckTrustLevel(webState, nullptr)) {
    completion(NO);
    return;
  }

  // Suggestions are available for the username field of the password form.
  const base::string16& pendingFormName = formData_->name;
  const base::string16& pendingFieldName = formData_->username_field.name;
  completion(base::SysNSStringToUTF16(formName) == pendingFormName &&
             base::SysNSStringToUTF16(fieldName) == pendingFieldName);
}

- (void)retrieveSuggestionsForForm:(NSString*)formName
                             field:(NSString*)fieldName
                              type:(NSString*)type
                        typedValue:(NSString*)typedValue
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  DCHECK(GetPageURLAndCheckTrustLevel(webState, nullptr));
  if (!formData_) {
    completion(@[], nil);
    return;
  }
  completion(BuildSuggestions(*formData_, typedValue), self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                   forField:(NSString*)fieldName
                       form:(NSString*)formName
          completionHandler:(SuggestionHandledCompletion)completion {
  const base::string16 username = base::SysNSStringToUTF16(suggestion.value);
  base::string16 password;
  if (FindMatchingUsername(*formData_, username, &password)) {
    [self fillPasswordForm:*formData_
              withUsername:username
                  password:password
         completionHandler:^(BOOL success) {
           completion();
         }];
  } else {
    completion();
  }
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
  // Not checking the return value, as empty action is valid.
  dictionary->GetString("action", &action);
  GURL actionUrl = action.empty() ? originUrl : originUrl.Resolve(action);
  if (!actionUrl.is_valid())
    return NO;
  form->action = stripURL(actionUrl);

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
      if (values[0] == values[1]) {
        // Treat two identical passwords as a single password.
        form->password_element = elements[0];
        form->password_value = values[0];
      } else {
        // Assume first is old password, second is new (no choice but to guess).
        form->new_password_element = elements[0];
        form->new_password_value = values[0];
        form->password_element = elements[1];
        form->password_value = values[1];
      }
      break;
      default:
        if (values[0] == values[1] && values[0] == values[2]) {
          // All three passwords the same? Just treat as one and hope.
          form->password_element = elements[0];
          form->password_value = values[0];
        } else if (values[0] == values[1]) {
          // Two the same and one different -> old password is the duplicated
          // one.
          form->new_password_element = elements[0];
          form->new_password_value = values[0];
          form->password_element = elements[2];
          form->password_value = values[2];
        } else if (values[1] == values[2]) {
          // Two the same and one different -> new password is the duplicated
          // one.
          form->new_password_element = elements[0];
          form->new_password_value = values[0];
          form->password_element = elements[1];
          form->password_value = values[1];
        } else {
          // Three different passwords, or first and last match with middle
          // different. No idea which is which, so no luck.
        }
        break;
    }
  }

  // Fill in as much data about the fields as is required for password
  // generation.
  const base::ListValue* fieldList = nullptr;
  if (!dictionary->GetList("fields", &fieldList))
    return NO;
  for (size_t i = 0; i < fieldList->GetSize(); ++i) {
    const base::DictionaryValue* fieldDictionary = nullptr;
    if (!fieldList->GetDictionary(i, &fieldDictionary))
      return NO;
    base::string16 element;
    base::string16 type;
    if (!fieldDictionary->GetString("element", &element) ||
        !fieldDictionary->GetString("type", &type)) {
      return NO;
    }
    autofill::FormFieldData field;
    field.name = std::move(element);
    field.form_control_type = base::UTF16ToUTF8(type);
    form->form_data.fields.push_back(field);
  }

  return YES;
}

- (void)showSavePasswordInfoBar:
    (std::unique_ptr<PasswordFormManager>)formToSave {
  if (!webStateObserverBridge_ || !webStateObserverBridge_->web_state())
    return;

  bool isSmartLockBrandingEnabled = false;
  if (self.browserState) {
    sync_driver::SyncService* sync_service =
        IOSChromeProfileSyncServiceFactory::GetForBrowserState(
            self.browserState);
    isSmartLockBrandingEnabled =
        password_bubble_experiment::IsSmartLockBrandingSavePromptEnabled(
            sync_service);
  }
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webStateObserverBridge_->web_state());
  IOSChromeSavePasswordInfoBarDelegate::Create(
      isSmartLockBrandingEnabled, infoBarManager, std::move(formToSave));
}

- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
            withUsername:(const base::string16&)username
                password:(const base::string16&)password
       completionHandler:(void (^)(BOOL))completionHandler {
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
  formData_.reset(new autofill::PasswordFormFillData(formData));

  // Don't fill immediately if waiting for the user to type a username.
  if (formData_->wait_for_username) {
    if (completionHandler)
      completionHandler(NO);
    return;
  }

  [self fillPasswordForm:*formData_
            withUsername:formData_->username_field.value
                password:formData_->password_field.value
       completionHandler:completionHandler];
}

- (PasswordGenerationAgent*)passwordGenerationAgent {
  return passwordGenerationAgent_.get();
}

- (PasswordGenerationManager*)passwordGenerationManager {
  return passwordGenerationManager_.get();
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

@end
