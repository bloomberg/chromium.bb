// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_controller.h"

#include <memory>
#include <vector>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/autofill/autofill_agent.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/autofill/autofill_client_ios.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/web_state/web_state.h"
#import "testing/gtest_mac.h"
#include "ui/base/test/ios/ui_view_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Real FormSuggestionController is wrapped to register the addition of
// suggestions.
@interface TestSuggestionController : FormSuggestionController

@property(nonatomic, copy) NSArray* suggestions;
@property(nonatomic, assign) BOOL suggestionRetrievalComplete;

@end

@implementation TestSuggestionController

@synthesize suggestions = _suggestions;
@synthesize suggestionRetrievalComplete = _suggestionRetrievalComplete;

- (void)retrieveSuggestionsForFormNamed:(const std::string&)formName
                              fieldName:(const std::string&)fieldName
                                   type:(const std::string&)type
                               webState:(web::WebState*)webState {
  self.suggestionRetrievalComplete = NO;
  [super retrieveSuggestionsForFormNamed:formName
                               fieldName:fieldName
                                    type:type
                                webState:webState];
}

- (void)updateKeyboardWithSuggestions:(NSArray*)suggestions {
  self.suggestions = suggestions;
  self.suggestionRetrievalComplete = YES;
}

- (void)onNoSuggestionsAvailable {
  self.suggestionRetrievalComplete = YES;
};

@end

namespace autofill {

namespace {

// The profile-type form used by tests.
NSString* const kProfileFormHtml =
    @"<form action='/submit' method='post'>"
     "Name <input type='text' name='name'>"
     "Address <input type='text' name='address'>"
     "City <input type='text' name='city'>"
     "State <input type='text' name='state'>"
     "Zip <input type='text' name='zip'>"
     "<input type='submit' id='submit' value='Submit'>"
     "</form>";

// A minimal form with a name.
NSString* const kMinimalFormWithNameHtml = @"<form id='form1'>"
                                            "<input name='name'>"
                                            "<input name='address'>"
                                            "<input name='city'>"
                                            "</form>";

// The key/value-type form used by tests.
NSString* const kKeyValueFormHtml =
    @"<form action='/submit' method='post'>"
     "Greeting <input type='text' name='greeting'>"
     "Dummy field <input type='text' name='dummy'>"
     "<input type='submit' id='submit' value='Submit'>"
     "</form>";

// The credit card-type form used by tests.
NSString* const kCreditCardFormHtml =
    @"<form action='/submit' method='post'>"
     "Name on card: <input type='text' name='name'>"
     "Credit card number: <input type='text' name='CCNo'>"
     "Expiry Month: <input type='text' name='CCExpiresMonth'>"
     "Expiry Year: <input type='text' name='CCExpiresYear'>"
     "<input type='submit' id='submit' value='Submit'>"
     "</form>";

// An HTML page without a card-type form.
static NSString* kNoCreditCardFormHtml =
    @"<h2>The rain in Spain stays <i>mainly</i> in the plain.</h2>";

// A credit card-type form with the autofocus attribute (which is detected at
// page load).
NSString* const kCreditCardAutofocusFormHtml =
    @"<form><input type=\"text\" autofocus autocomplete=\"cc-number\"></form>";

// Experiment preference key.
NSString* const kAutofillVisible = @"AutofillVisible";

// FAIL if a field with the supplied |name| and |fieldType| is not present on
// the |form|.
void CheckField(const FormStructure& form,
                ServerFieldType fieldType,
                const char* name) {
  for (const auto& field : form) {
    if (field->heuristic_type() == fieldType) {
      EXPECT_EQ(base::UTF8ToUTF16(name), field->unique_name());
      return;
    }
  }
  FAIL() << "Missing field " << name;
}

// WebDataServiceConsumer for receving vectors of strings and making them
// available to tests.
class TestConsumer : public WebDataServiceConsumer {
 public:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override {
    DCHECK_EQ(result->GetType(), AUTOFILL_VALUE_RESULT);
    result_ = static_cast<WDResult<std::vector<base::string16>>*>(result.get())
                  ->GetValue();
  }
  std::vector<base::string16> result_;
};

// Text fixture to test autofill.
class AutofillControllerTest : public ChromeWebTest {
 public:
  AutofillControllerTest() = default;
  ~AutofillControllerTest() override {}

 protected:
  void SetUp() override;
  void TearDown() override;
  void SetUpForSuggestions(NSString* data);

  // Adds key value data to the Personal Data Manager and loads test page.
  void SetUpKeyValueData();

  // Blocks until suggestion retrieval has completed.
  void WaitForSuggestionRetrieval();

  // Fails if the specified metric was not registered the given number of times.
  void ExpectMetric(const std::string& histogram_name, int sum);

  // Fails if the specified user happiness metric was not registered.
  void ExpectHappinessMetric(AutofillMetrics::UserHappinessMetric metric);

  TestSuggestionController* suggestion_controller() {
    return suggestion_controller_;
  }

 private:
  // Weak pointer to AutofillAgent, owned by the AutofillController.
  __weak AutofillAgent* autofill_agent_;

  // Histogram tester for these tests.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  // Retrieves suggestions according to form events.
  TestSuggestionController* suggestion_controller_;

  // Retrieves accessory views according to form events.
  FormInputAccessoryViewController* accessory_controller_;

  // Manages autofill for a single page.
  AutofillController* autofill_controller_;

  DISALLOW_COPY_AND_ASSIGN(AutofillControllerTest);
};

void AutofillControllerTest::SetUp() {
  ChromeWebTest::SetUp();

  // Profile import requires a PersonalDataManager which itself needs the
  // WebDataService; this is not initialized on a TestChromeBrowserState by
  // default.
  chrome_browser_state_->CreateWebDataService();
  // Enable autofill experiment.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:YES forKey:kAutofillVisible];

  AutofillAgent* agent =
      [[AutofillAgent alloc] initWithBrowserState:chrome_browser_state_.get()
                                         webState:web_state()];
  autofill_agent_ = agent;
  InfoBarManagerImpl::CreateForWebState(web_state());
  autofill_controller_ = [[AutofillController alloc]
           initWithBrowserState:chrome_browser_state_.get()
                       webState:web_state()
                  autofillAgent:autofill_agent_
      passwordGenerationManager:nullptr
                downloadEnabled:NO];
  suggestion_controller_ = [[TestSuggestionController alloc]
      initWithWebState:web_state()
             providers:@[ [autofill_controller_ suggestionProvider] ]];
  accessory_controller_ = [[FormInputAccessoryViewController alloc]
      initWithWebState:web_state()
             providers:@[ [suggestion_controller_ accessoryViewProvider] ]];
  histogram_tester_.reset(new base::HistogramTester());
}

void AutofillControllerTest::TearDown() {
  [autofill_controller_ detachFromWebState];
  [suggestion_controller_ detachFromWebState];

  ChromeWebTest::TearDown();
}

void AutofillControllerTest::WaitForSuggestionRetrieval() {
  // Wait for the message queue to ensure that JS events fired in the tests
  // trigger TestSuggestionController's retrieveSuggestionsForFormNamed: method
  // and set suggestionRetrievalComplete to NO.
  WaitForBackgroundTasks();

  // Now we can wait for suggestionRetrievalComplete to be set to YES.
  WaitForCondition(^bool {
    return [suggestion_controller() suggestionRetrievalComplete];
  });
}

void AutofillControllerTest::ExpectMetric(const std::string& histogram_name,
                                          int sum) {
  histogram_tester_->ExpectBucketCount(histogram_name, sum, 1);
}

void AutofillControllerTest::ExpectHappinessMetric(
    AutofillMetrics::UserHappinessMetric metric) {
  histogram_tester_->ExpectBucketCount("Autofill.UserHappiness", metric, 1);
}

// Checks that viewing an HTML page containing a form results in the form being
// registered as a FormStructure by the AutofillManager.
TEST_F(AutofillControllerTest, ReadForm) {
  AutofillManager* autofill_manager =
      AutofillDriverIOS::FromWebState(web_state())->autofill_manager();
  EXPECT_TRUE(autofill_manager->GetFormStructures().empty())
      << "Forms are registered at beginning";
  LoadHtml(kProfileFormHtml);
  const std::vector<std::unique_ptr<FormStructure>>& forms =
      autofill_manager->GetFormStructures();
  ASSERT_EQ(1U, forms.size());
  CheckField(*forms[0], NAME_FULL, "name_1");
  CheckField(*forms[0], ADDRESS_HOME_LINE1, "address_1");
  CheckField(*forms[0], ADDRESS_HOME_CITY, "city_1");
  CheckField(*forms[0], ADDRESS_HOME_STATE, "state_1");
  CheckField(*forms[0], ADDRESS_HOME_ZIP, "zip_1");
  ExpectMetric("Autofill.IsEnabled.PageLoad", 1);
  ExpectHappinessMetric(AutofillMetrics::FORMS_LOADED);
};

// Checks that viewing an HTML page containing a form with an 'id' results in
// the form being registered as a FormStructure by the AutofillManager, and the
// name is correctly set.
TEST_F(AutofillControllerTest, ReadFormName) {
  AutofillManager* autofill_manager =
      AutofillDriverIOS::FromWebState(web_state())->autofill_manager();
  LoadHtml(kMinimalFormWithNameHtml);
  const std::vector<std::unique_ptr<FormStructure>>& forms =
      autofill_manager->GetFormStructures();
  ASSERT_EQ(1U, forms.size());
  EXPECT_EQ(base::UTF8ToUTF16("form1"), forms[0]->ToFormData().name);
};

// Checks that an HTML page containing a profile-type form which is submitted
// with scripts (simulating user form submission) results in a profile being
// successfully imported into the PersonalDataManager.
TEST_F(AutofillControllerTest, ProfileImport) {
  AutofillManager* autofill_manager =
      AutofillDriverIOS::FromWebState(web_state())->autofill_manager();
  PersonalDataManager* personal_data_manager =
      autofill_manager->client()->GetPersonalDataManager();
  // Check there are no registered profiles already.
  EXPECT_EQ(0U, personal_data_manager->GetProfiles().size());
  LoadHtml(kProfileFormHtml);
  ExecuteJavaScript(@"document.forms[0].name.value = 'Homer Simpson'");
  ExecuteJavaScript(@"document.forms[0].address.value = '123 Main Street'");
  ExecuteJavaScript(@"document.forms[0].city.value = 'Springfield'");
  ExecuteJavaScript(@"document.forms[0].state.value = 'IL'");
  ExecuteJavaScript(@"document.forms[0].zip.value = '55123'");
  ExecuteJavaScript(@"submit.click()");
  WaitForCondition(^bool {
    return personal_data_manager->GetProfiles().size();
  });
  const std::vector<AutofillProfile*>& profiles =
      personal_data_manager->GetProfiles();
  if (profiles.size() != 1)
    FAIL() << "Not exactly one profile found after attempted import";
  const AutofillProfile& profile = *profiles[0];
  EXPECT_EQ(base::UTF8ToUTF16("Homer Simpson"),
            profile.GetInfo(AutofillType(NAME_FULL), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("123 Main Street"),
            profile.GetInfo(AutofillType(ADDRESS_HOME_LINE1), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("Springfield"),
            profile.GetInfo(AutofillType(ADDRESS_HOME_CITY), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("IL"),
            profile.GetInfo(AutofillType(ADDRESS_HOME_STATE), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("55123"),
            profile.GetInfo(AutofillType(ADDRESS_HOME_ZIP), "en-US"));
};

void AutofillControllerTest::SetUpForSuggestions(NSString* data) {
  AutofillManager* autofill_manager =
      AutofillDriverIOS::FromWebState(web_state())->autofill_manager();
  PersonalDataManager* personal_data_manager =
      autofill_manager->client()->GetPersonalDataManager();
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  profile.SetRawInfo(NAME_FULL, base::UTF8ToUTF16("Homer Simpson"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, base::UTF8ToUTF16("123 Main Street"));
  profile.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16("Springfield"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("IL"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("55123"));
  EXPECT_EQ(0U, personal_data_manager->GetProfiles().size());
  personal_data_manager->SaveImportedProfile(profile);
  EXPECT_EQ(1U, personal_data_manager->GetProfiles().size());
  LoadHtml(data);
  WaitForBackgroundTasks();
}

// Checks that focusing on a text element of a profile-type form will result in
// suggestions being sent to the AutofillAgent, once data has been loaded into a
// test data manager.
TEST_F(AutofillControllerTest, ProfileSuggestions) {
  SetUpForSuggestions(kProfileFormHtml);
  ui::test::uiview_utils::ForceViewRendering(web_state()->GetView());
  ExecuteJavaScript(@"document.forms[0].name.focus()");
  WaitForSuggestionRetrieval();
  ExpectMetric("Autofill.AddressSuggestionsCount", 1);
  ExpectHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Homer Simpson", suggestion.value);
};

// Tests that the system is able to offer suggestions for an anonymous form when
// there is another anonymous form on the page.
TEST_F(AutofillControllerTest, ProfileSuggestionsTwoAnonymousForms) {
  SetUpForSuggestions(
      [NSString stringWithFormat:@"%@%@", kProfileFormHtml, kProfileFormHtml]);
  ui::test::uiview_utils::ForceViewRendering(web_state()->GetView());
  ExecuteJavaScript(@"document.forms[0].name.focus()");
  WaitForSuggestionRetrieval();
  ExpectMetric("Autofill.AddressSuggestionsCount", 1);
  ExpectHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Homer Simpson", suggestion.value);
};

// Checks that focusing on a select element in a profile-type form will result
// in suggestions being sent to the AutofillAgent, once data has been loaded
// into a test data manager.
TEST_F(AutofillControllerTest, ProfileSuggestionsFromSelectField) {
  SetUpForSuggestions(kProfileFormHtml);
  WaitForBackgroundTasks();
  ui::test::uiview_utils::ForceViewRendering(web_state()->GetView());
  ExecuteJavaScript(@"document.forms[0].state.focus()");
  WaitForSuggestionRetrieval();
  ExpectMetric("Autofill.AddressSuggestionsCount", 1);
  ExpectHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"IL", suggestion.value);
};

// Checks that multiple profiles will offer a matching number of suggestions.
TEST_F(AutofillControllerTest, MultipleProfileSuggestions) {
  AutofillManager* autofill_manager =
      AutofillDriverIOS::FromWebState(web_state())->autofill_manager();
  PersonalDataManager* personal_data_manager =
      autofill_manager->client()->GetPersonalDataManager();
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  profile.SetRawInfo(NAME_FULL, base::UTF8ToUTF16("Homer Simpson"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, base::UTF8ToUTF16("123 Main Street"));
  profile.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16("Springfield"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("IL"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("55123"));
  EXPECT_EQ(0U, personal_data_manager->GetProfiles().size());
  personal_data_manager->SaveImportedProfile(profile);
  EXPECT_EQ(1U, personal_data_manager->GetProfiles().size());
  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com/");
  profile2.SetRawInfo(NAME_FULL, base::UTF8ToUTF16("Larry Page"));
  profile2.SetRawInfo(ADDRESS_HOME_LINE1,
                      base::UTF8ToUTF16("1600 Amphitheatre Parkway"));
  profile2.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16("Mountain View"));
  profile2.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("CA"));
  profile2.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("94043"));
  personal_data_manager->SaveImportedProfile(profile2);
  EXPECT_EQ(2U, personal_data_manager->GetProfiles().size());
  LoadHtml(kProfileFormHtml);
  WaitForBackgroundTasks();
  ui::test::uiview_utils::ForceViewRendering(web_state()->GetView());
  ExecuteJavaScript(@"document.forms[0].name.focus()");
  WaitForSuggestionRetrieval();
  ExpectMetric("Autofill.AddressSuggestionsCount", 2);
  ExpectHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);
  EXPECT_EQ(2U, [suggestion_controller() suggestions].count);
}

// Check that an HTML page containing a key/value type form which is submitted
// with scripts (simulating user form submission) results in data being
// successfully registered.
TEST_F(AutofillControllerTest, KeyValueImport) {
  LoadHtml(kKeyValueFormHtml);
  ExecuteJavaScript(@"document.forms[0].greeting.value = 'Hello'");
  scoped_refptr<AutofillWebDataService> web_data_service =
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  __block TestConsumer consumer;
  const int limit = 1;
  consumer.result_ = {base::ASCIIToUTF16("Should"), base::ASCIIToUTF16("get"),
                      base::ASCIIToUTF16("overwritten")};
  web_data_service->GetFormValuesForElementName(
      base::UTF8ToUTF16("greeting"), base::string16(), limit, &consumer);
  WaitForBackgroundTasks();
  // No value should be returned before anything is loaded via form submission.
  ASSERT_EQ(0U, consumer.result_.size());
  ExecuteJavaScript(@"submit.click()");
  WaitForCondition(^bool {
    web_data_service->GetFormValuesForElementName(
        base::UTF8ToUTF16("greeting"), base::string16(), limit, &consumer);
    return consumer.result_.size();
  });
  WaitForBackgroundTasks();
  // One result should be returned, matching the filled value.
  ASSERT_EQ(1U, consumer.result_.size());
  EXPECT_EQ(base::UTF8ToUTF16("Hello"), consumer.result_[0]);
};

void AutofillControllerTest::SetUpKeyValueData() {
  scoped_refptr<AutofillWebDataService> web_data_service =
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  // Load value into database.
  std::vector<FormFieldData> values;
  FormFieldData fieldData;
  fieldData.name = base::UTF8ToUTF16("greeting");
  fieldData.value = base::UTF8ToUTF16("Bonjour");
  values.push_back(fieldData);
  web_data_service->AddFormFields(values);

  // Load test page.
  LoadHtml(kKeyValueFormHtml);
  WaitForBackgroundTasks();
}

// Checks that focusing on an element of a key/value type form then typing the
// first letter of a suggestion will result in suggestions being sent to the
// AutofillAgent, once data has been loaded into a test data manager.
TEST_F(AutofillControllerTest, KeyValueSuggestions) {
  SetUpKeyValueData();

  // Focus element.
  ExecuteJavaScript(@"document.forms[0].greeting.value='B'");
  ExecuteJavaScript(@"document.forms[0].greeting.focus()");
  WaitForSuggestionRetrieval();
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Bonjour", suggestion.value);
};

// Checks that typing events (simulated in script) result in suggestions. Note
// that the field is not explictly focused before typing starts; this can happen
// in practice and should not result in a crash or incorrect behavior.
TEST_F(AutofillControllerTest, KeyValueTypedSuggestions) {
  SetUpKeyValueData();
  ExecuteJavaScript(@"document.forms[0].greeting.select()");
  ExecuteJavaScript(@"event = document.createEvent('TextEvent');");
  ExecuteJavaScript(
      @"event.initTextEvent('textInput', true, true, window, 'B');");
  ExecuteJavaScript(@"document.forms[0].greeting.dispatchEvent(event);");
  WaitForSuggestionRetrieval();
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Bonjour", suggestion.value);
}

// Checks that focusing on and typing on one field, then changing focus before
// typing again, result in suggestions.
TEST_F(AutofillControllerTest, KeyValueFocusChange) {
  SetUpKeyValueData();

  // Focus the dummy field and confirm no suggestions are presented.
  ExecuteJavaScript(@"document.forms[0].dummy.focus()");
  WaitForSuggestionRetrieval();
  EXPECT_EQ(0U, [suggestion_controller() suggestions].count);

  // Enter 'B' in the dummy field and confirm no suggestions are presented.
  ExecuteJavaScript(@"event = document.createEvent('TextEvent');");
  ExecuteJavaScript(
      @"event.initTextEvent('textInput', true, true, window, 'B');");
  ExecuteJavaScript(@"document.forms[0].dummy.dispatchEvent(event);");
  WaitForSuggestionRetrieval();
  EXPECT_EQ(0U, [suggestion_controller() suggestions].count);

  // Enter 'B' in the greeting field and confirm that one suggestion ("Bonjour")
  // is presented.
  ExecuteJavaScript(@"document.forms[0].greeting.select()");
  ExecuteJavaScript(@"event = document.createEvent('TextEvent');");
  ExecuteJavaScript(
      @"event.initTextEvent('textInput', true, true, window, 'B');");
  ExecuteJavaScript(@"document.forms[0].greeting.dispatchEvent(event);");
  WaitForSuggestionRetrieval();
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Bonjour", suggestion.value);
}

// Checks that focusing on an element of a key/value type form without typing
// won't result in suggestions being sent to the AutofillAgent, once data has
// been loaded into a test data manager.
TEST_F(AutofillControllerTest, NoKeyValueSuggestionsWithoutTyping) {
  SetUpKeyValueData();
  // Focus element.
  ExecuteJavaScript(@"document.forms[0].greeting.focus()");
  WaitForSuggestionRetrieval();
  EXPECT_EQ(0U, [suggestion_controller() suggestions].count);
}

// Checks that an HTML page containing a credit card-type form which is
// submitted with scripts (simulating user form submission) results in a credit
// card being successfully imported into the PersonalDataManager.
TEST_F(AutofillControllerTest, CreditCardImport) {
  InfoBarManagerImpl::CreateForWebState(web_state());
  AutofillManager* autofill_manager =
      AutofillDriverIOS::FromWebState(web_state())->autofill_manager();
  PersonalDataManager* personal_data_manager =
      autofill_manager->client()->GetPersonalDataManager();
  // Check there are no registered profiles already.
  EXPECT_EQ(0U, personal_data_manager->GetCreditCards().size());
  LoadHtml(kCreditCardFormHtml);
  ExecuteJavaScript(@"document.forms[0].name.value = 'Superman'");
  ExecuteJavaScript(@"document.forms[0].CCNo.value = '4000-4444-4444-4444'");
  ExecuteJavaScript(@"document.forms[0].CCExpiresMonth.value = '11'");
  ExecuteJavaScript(@"document.forms[0].CCExpiresYear.value = '2999'");
  ExecuteJavaScript(@"submit.click()");
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  base::test::ios::WaitUntilCondition(^bool() {
    return infobar_manager->infobar_count();
  });
  ExpectMetric("Autofill.CreditCardInfoBar.Local",
               AutofillMetrics::INFOBAR_SHOWN);
  ASSERT_EQ(1U, infobar_manager->infobar_count());
  infobars::InfoBarDelegate* infobar =
      infobar_manager->infobar_at(0)->delegate();
  ConfirmInfoBarDelegate* confirm_infobar = infobar->AsConfirmInfoBarDelegate();
  confirm_infobar->Accept();
  const std::vector<CreditCard*>& credit_cards =
      personal_data_manager->GetCreditCards();
  ASSERT_EQ(1U, credit_cards.size());
  const CreditCard& credit_card = *credit_cards[0];
  EXPECT_EQ(base::UTF8ToUTF16("Superman"),
            credit_card.GetInfo(AutofillType(CREDIT_CARD_NAME_FULL), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("4000444444444444"),
            credit_card.GetInfo(AutofillType(CREDIT_CARD_NUMBER), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("11"),
            credit_card.GetInfo(AutofillType(CREDIT_CARD_EXP_MONTH), "en-US"));
  EXPECT_EQ(
      base::UTF8ToUTF16("2999"),
      credit_card.GetInfo(AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), "en-US"));
};

// Checks that an HTTP page containing a credit card results in a navigation
// entry with the "credit card displayed" bit set to true.
// TODO(crbug.com/689082): disabled due to flakyness.
TEST_F(AutofillControllerTest, DISABLED_HttpCreditCard) {
  LoadHtml(kCreditCardAutofocusFormHtml, GURL("http://chromium.test"));

  web::SSLStatus ssl_status =
      web_state()->GetNavigationManager()->GetLastCommittedItem()->GetSSL();
  EXPECT_TRUE(ssl_status.content_status &
              web::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
};

// Checks that an HTTP page without a credit card form does not result in a
// navigation entry with the "credit card displayed" bit set to true.
TEST_F(AutofillControllerTest, HttpNoCreditCard) {
  LoadHtml(kNoCreditCardFormHtml, GURL("http://chromium.test"));

  web::SSLStatus ssl_status =
      web_state()->GetNavigationManager()->GetLastCommittedItem()->GetSSL();
  EXPECT_FALSE(ssl_status.content_status &
               web::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
};

// Checks that an HTTPS page containing a credit card form does not result in a
// navigation entry with the "credit card displayed" bit set to true.
TEST_F(AutofillControllerTest, HttpsCreditCard) {
  LoadHtml(kCreditCardAutofocusFormHtml, GURL("https://chromium.test"));

  web::SSLStatus ssl_status =
      web_state()->GetNavigationManager()->GetLastCommittedItem()->GetSSL();
  EXPECT_FALSE(ssl_status.content_status &
               web::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
};

// Checks that an HTTPS page without a credit card form does not result in a
// navigation entry with the "credit card displayed" bit set to true.
TEST_F(AutofillControllerTest, HttpsNoCreditCard) {
  LoadHtml(kNoCreditCardFormHtml, GURL("https://chromium.test"));

  web::SSLStatus ssl_status =
      web_state()->GetNavigationManager()->GetLastCommittedItem()->GetSSL();
  EXPECT_FALSE(ssl_status.content_status &
               web::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
};

}  // namespace

}  // namespace autofill
