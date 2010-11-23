// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"

// Test class for verifying proper form structure as determined by AutoFill
// heuristics.  After a test loads HTML content with a call to |NavigateToURL|
// the |AutoFillManager| associated with the tab contents is queried for the
// form structures that were loaded and parsed.
// These form structures are serialized to string form and compared with
// expected results.
class FormStructureBrowserTest : public InProcessBrowserTest {
 public:
  FormStructureBrowserTest() {}
  virtual ~FormStructureBrowserTest() {}

 protected:
  // Returns a vector of form structure objects associated with the given
  // |autofill_manager|.
  const std::vector<FormStructure*>& GetFormStructures(
      const AutoFillManager& autofill_manager);

  // Serializes the given form structures in |forms| to string form.
  const std::string FormStructuresToString(
      const std::vector<FormStructure*>& forms);

 private:
  // A helper utility for converting an |AutoFillFieldType| to string form.
  const std::string AutoFillFieldTypeToString(AutoFillFieldType type);

  DISALLOW_COPY_AND_ASSIGN(FormStructureBrowserTest);
};

const std::vector<FormStructure*>& FormStructureBrowserTest::GetFormStructures(
  const AutoFillManager& autofill_manager) {
  return autofill_manager.form_structures_.get();
}

const std::string FormStructureBrowserTest::FormStructuresToString(
    const std::vector<FormStructure*>& forms) {
  std::string forms_string;
  for (std::vector<FormStructure*>::const_iterator iter = forms.begin();
       iter != forms.end();
       ++iter) {
    forms_string += (*iter)->source_url().spec();
    forms_string += "\n";

    for (std::vector<AutoFillField*>::const_iterator field_iter =
            (*iter)->begin();
         field_iter != (*iter)->end();
         ++field_iter) {
      // The field list is NULL-terminated.  Exit loop when at the end.
      if (!*field_iter)
        break;
      forms_string += AutoFillFieldTypeToString((*field_iter)->type());
      forms_string += "\n";
    }
  }
  return forms_string;
}

const std::string FormStructureBrowserTest::AutoFillFieldTypeToString(
    AutoFillFieldType type) {
  switch (type) {
    case NO_SERVER_DATA:
      return "NO_SERVER_DATA";
    case UNKNOWN_TYPE:
      return "UNKNOWN_TYPE";
    case EMPTY_TYPE:
      return "EMPTY_TYPE";
    case NAME_FIRST:
      return "NAME_FIRST";
    case NAME_MIDDLE:
      return "NAME_MIDDLE";
    case NAME_LAST:
      return "NAME_LAST";
    case NAME_MIDDLE_INITIAL:
      return "NAME_MIDDLE_INITIAL";
    case NAME_FULL:
      return "NAME_FULL";
    case NAME_SUFFIX:
      return "NAME_SUFFIX";
    case EMAIL_ADDRESS:
      return "EMAIL_ADDRESS";
    case PHONE_HOME_NUMBER:
      return "PHONE_HOME_NUMBER";
    case PHONE_HOME_CITY_CODE:
      return "PHONE_HOME_CITY_CODE";
    case PHONE_HOME_COUNTRY_CODE:
      return "PHONE_HOME_COUNTRY_CODE";
    case PHONE_HOME_CITY_AND_NUMBER:
      return "PHONE_HOME_CITY_AND_NUMBER";
    case PHONE_HOME_WHOLE_NUMBER:
      return "PHONE_HOME_WHOLE_NUMBER";
    case PHONE_FAX_NUMBER:
      return "PHONE_FAX_NUMBER";
    case PHONE_FAX_CITY_CODE:
      return "PHONE_FAX_CITY_CODE";
    case PHONE_FAX_COUNTRY_CODE:
      return "PHONE_FAX_COUNTRY_CODE";
    case PHONE_FAX_CITY_AND_NUMBER:
      return "PHONE_FAX_CITY_AND_NUMBER";
    case PHONE_FAX_WHOLE_NUMBER:
      return "PHONE_FAX_WHOLE_NUMBER";
    case ADDRESS_HOME_LINE1:
      return "ADDRESS_HOME_LINE1";
    case ADDRESS_HOME_LINE2:
      return "ADDRESS_HOME_LINE2";
    case ADDRESS_HOME_APT_NUM:
      return "ADDRESS_HOME_APT_NUM";
    case ADDRESS_HOME_CITY:
      return "ADDRESS_HOME_CITY";
    case ADDRESS_HOME_STATE:
      return "ADDRESS_HOME_STATE";
    case ADDRESS_HOME_ZIP:
      return "ADDRESS_HOME_ZIP";
    case ADDRESS_HOME_COUNTRY:
      return "ADDRESS_HOME_COUNTRY";
    case ADDRESS_BILLING_LINE1:
      return "ADDRESS_BILLING_LINE1";
    case ADDRESS_BILLING_LINE2:
      return "ADDRESS_BILLING_LINE2";
    case ADDRESS_BILLING_APT_NUM:
      return "ADDRESS_BILLING_APT_NUM";
    case ADDRESS_BILLING_CITY:
      return "ADDRESS_BILLING_CITY";
    case ADDRESS_BILLING_STATE:
      return "ADDRESS_BILLING_STATE";
    case ADDRESS_BILLING_ZIP:
      return "ADDRESS_BILLING_ZIP";
    case ADDRESS_BILLING_COUNTRY:
      return "ADDRESS_BILLING_COUNTRY";
    case CREDIT_CARD_NAME:
      return "CREDIT_CARD_NAME";
    case CREDIT_CARD_NUMBER:
      return "CREDIT_CARD_NUMBER";
    case CREDIT_CARD_EXP_MONTH:
      return "CREDIT_CARD_EXP_MONTH";
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_2_DIGIT_YEAR";
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_4_DIGIT_YEAR";
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR";
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR";
    case CREDIT_CARD_TYPE:
      return "CREDIT_CARD_TYPE";
    case CREDIT_CARD_VERIFICATION_CODE:
      return "CREDIT_CARD_VERIFICATION_CODE";
    case COMPANY_NAME:
      return "COMPANY_NAME";
    default:
      NOTREACHED() << "Invalid AutoFillFieldType value.";
  }

  return std::string();
}

IN_PROC_BROWSER_TEST_F(FormStructureBrowserTest, BasicFormStructure) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html;charset=utf-8,"
                      "<form action=\"http://www.google.com/\" method=\"POST\">"
                      "<label for=\"firstname\">First name:</label>"
                      " <input type=\"text\" id=\"firstname\"/><br />"
                      "<label for=\"lastname\">Last name:</label>"
                      " <input type=\"text\" id=\"lastname\" /><br />"
                      "<label for=\"address1\">Address line 1:</label>"
                      " <input type=\"text\" id=\"address1\" /><br />"
                      "<label for=\"address2\">Address line 2:</label>"
                      " <input type=\"text\" id=\"address2\" /><br />"
                      "<label for=\"city\">City:</label>"
                      " <input type=\"text\" id=\"city\" /><br />"
                      "</form>")));

  ASSERT_NO_FATAL_FAILURE(ui_test_utils::ClickOnView(browser(),
                                                     VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  AutoFillManager* autofill_manager =
      browser()->GetSelectedTabContents()->GetAutoFillManager();
  ASSERT_NE(static_cast<AutoFillManager*>(NULL), autofill_manager);
  std::vector<FormStructure*> forms = GetFormStructures(*autofill_manager);
  std::string expected("data:text/html;charset=utf-8,"
                       "<form action=\"http://www.google.com/\""
                       " method=\"POST\">"
                       "<label for=\"firstname\">First name:</label>"
                       " <input type=\"text\" id=\"firstname\"/><br />"
                       "<label for=\"lastname\">Last name:</label>"
                       " <input type=\"text\" id=\"lastname\" /><br />"
                       "<label for=\"address1\">Address line 1:</label>"
                       " <input type=\"text\" id=\"address1\" /><br />"
                       "<label for=\"address2\">Address line 2:</label>"
                       " <input type=\"text\" id=\"address2\" /><br />"
                       "<label for=\"city\">City:</label>"
                       " <input type=\"text\" id=\"city\" /><br />"
                       "</form>\n"
                       "NAME_FIRST\n"
                       "NAME_LAST\n"
                       "ADDRESS_HOME_LINE1\n"
                       "ADDRESS_HOME_LINE2\n"
                       "ADDRESS_HOME_CITY\n");

  EXPECT_EQ(expected, FormStructureBrowserTest::FormStructuresToString(forms));
}
