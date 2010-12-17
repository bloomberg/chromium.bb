// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"

namespace {

FilePath GetInputFileDirectory() {
  FilePath test_data_dir_;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("autofill_heuristics")
                                 .AppendASCII("input");
  return test_data_dir_;
}

FilePath GetOutputFileDirectory() {
  FilePath test_data_dir_;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("autofill_heuristics")
                                 .AppendASCII("output");
  return test_data_dir_;
}

// Write |content| to |file|. Returns true on success.
bool WriteFile(const FilePath& file, const std::string& content) {
  int write_size = file_util::WriteFile(file, content.c_str(),
                                        content.length());
  return write_size == static_cast<int>(content.length());
}

// Convert |html| to URL format, and return the converted string.
const std::string ConvertToURLFormat(const std::string& html) {
  return std::string("data:text/html;charset=utf-8,") + html;
}

// Compare |output_file_source| with |form_string|. Returns true when they are
// identical.
bool CompareText(const std::string& output_file_source,
                 const std::string& form_string) {
  std::string output_file = output_file_source;
  std::string form = form_string;

  ReplaceSubstringsAfterOffset(&output_file, 0, "\r\n", " ");
  ReplaceSubstringsAfterOffset(&output_file, 0, "\r", " ");
  ReplaceSubstringsAfterOffset(&output_file, 0, "\n", " ");
  ReplaceSubstringsAfterOffset(&form, 0, "\n", " ");

  return (output_file == form);
}

}  // namespace

// Test class for verifying proper form structure as determined by AutoFill
// heuristics. A test inputs each form file(e.g. form_[language_code].html),
// loads its HTML content with a call to |NavigateToURL|, the |AutoFillManager|
// associated with the tab contents is queried for the form structures that
// were loaded and parsed. These form structures are serialized to string form.
// If this is the first time test is run, a gold test result file is generated
// in output directory, else the form structures are compared against the
// existing result file.
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

IN_PROC_BROWSER_TEST_F(FormStructureBrowserTest, HTMLFiles) {
  FilePath input_file_path = GetInputFileDirectory();
  file_util::FileEnumerator input_file_enumerator(input_file_path,
      false, file_util::FileEnumerator::FILES, FILE_PATH_LITERAL("*.html"));

  for (input_file_path = input_file_enumerator.Next();
       !input_file_path.empty();
       input_file_path = input_file_enumerator.Next()) {
    std::string input_file_source;

    ASSERT_TRUE(file_util::ReadFileToString(input_file_path,
                                            &input_file_source));
    input_file_source = ConvertToURLFormat(input_file_source);

    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
        browser(), GURL(input_file_source)));

    ASSERT_NO_FATAL_FAILURE(ui_test_utils::ClickOnView(browser(),
                                                       VIEW_ID_TAB_CONTAINER));
    ASSERT_TRUE(ui_test_utils::IsViewFocused(
        browser(),
        VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

    AutoFillManager* autofill_manager =
        browser()->GetSelectedTabContents()->GetAutoFillManager();
    ASSERT_NE(static_cast<AutoFillManager*>(NULL), autofill_manager);
    std::vector<FormStructure*> forms = GetFormStructures(*autofill_manager);

    FilePath output_file_directory = GetOutputFileDirectory();
    FilePath output_file_path = output_file_directory.Append(
        input_file_path.BaseName().StripTrailingSeparators().ReplaceExtension(
            FILE_PATH_LITERAL(".out")));

    std::string output_file_source;
    if (file_util::ReadFileToString(output_file_path, &output_file_source)) {
      ASSERT_TRUE(CompareText(
          output_file_source,
          FormStructureBrowserTest::FormStructuresToString(forms)));

    } else {
      ASSERT_TRUE(WriteFile(
          output_file_path,
          FormStructureBrowserTest::FormStructuresToString(forms)));
    }
  }
}
