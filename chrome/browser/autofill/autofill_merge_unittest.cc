// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/data_driven_test.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_data.h"

namespace {

const FilePath::CharType kTestName[] = FILE_PATH_LITERAL("merge");
const FilePath::CharType kFileNamePattern[] = FILE_PATH_LITERAL("*.in");

const char kFieldSeparator[] = ": ";
const char kProfileSeparator[] = "---";
const size_t kFieldOffset = arraysize(kFieldSeparator) - 1;

const AutofillFieldType kProfileFieldTypes[] = {
  NAME_FIRST,
  NAME_MIDDLE,
  NAME_LAST,
  EMAIL_ADDRESS,
  COMPANY_NAME,
  ADDRESS_HOME_LINE1,
  ADDRESS_HOME_LINE2,
  ADDRESS_HOME_CITY,
  ADDRESS_HOME_STATE,
  ADDRESS_HOME_ZIP,
  ADDRESS_HOME_COUNTRY,
  PHONE_HOME_NUMBER,
  PHONE_FAX_NUMBER,
};

// Serializes the |profiles| into a string.
std::string SerializeProfiles(const std::vector<AutoFillProfile*>& profiles) {
  std::string result;
  for (size_t i = 0; i < profiles.size(); ++i) {
    result += kProfileSeparator;
    result += "\n";
    for (size_t j = 0; j < arraysize(kProfileFieldTypes); ++j) {
      AutofillFieldType type = kProfileFieldTypes[j];
      result += AutoFillType::FieldTypeToString(type);
      result += kFieldSeparator;
      result += UTF16ToUTF8(profiles[i]->GetFieldText(AutoFillType(type)));
      result += "\n";
    }
  }

  return result;
}

class PersonalDataManagerMock : public PersonalDataManager {
 public:
  PersonalDataManagerMock();
  virtual ~PersonalDataManagerMock();

  // Reset the saved profiles.
  void Reset();

  // PersonalDataManager:
  virtual void SaveImportedProfile(const AutoFillProfile& profile) OVERRIDE;
  virtual const std::vector<AutoFillProfile*>& web_profiles() OVERRIDE;

 private:
  ScopedVector<AutoFillProfile> profiles_;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManagerMock);
};

PersonalDataManagerMock::PersonalDataManagerMock() : PersonalDataManager() {
}

PersonalDataManagerMock::~PersonalDataManagerMock() {
}

void PersonalDataManagerMock::Reset() {
  profiles_.reset();
}

void PersonalDataManagerMock::SaveImportedProfile(
    const AutoFillProfile& profile) {
  std::vector<AutoFillProfile> profiles;
  if (!MergeProfile(profile, profiles_.get(), &profiles))
    profiles_.push_back(new AutoFillProfile(profile));
}

const std::vector<AutoFillProfile*>& PersonalDataManagerMock::web_profiles() {
  return profiles_.get();
}

}  // namespace

// A data-driven test for verifying merging of AutoFill profiles. Each input is
// a structured dump of a set of implicitly detected autofill profiles. The
// corresponding output file is a dump of the saved profiles that result from
// importing the input profiles. The output file format is identical to the
// input format.
class AutoFillMergeTest : public testing::Test, public DataDrivenTest {
 protected:
  AutoFillMergeTest();
  virtual ~AutoFillMergeTest();

  // testing::Test:
  virtual void SetUp();

  // DataDrivenTest:
  virtual void GenerateResults(const std::string& input,
                               std::string* output) OVERRIDE;

  // Deserializes a set of AutoFill profiles from |profiles|, imports each
  // sequentially, and fills |merged_profiles| with the serialized result.
  void MergeProfiles(const std::string& profiles, std::string* merged_profiles);

  scoped_refptr<PersonalDataManagerMock> personal_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillMergeTest);
};

AutoFillMergeTest::AutoFillMergeTest() : DataDrivenTest() {
}

AutoFillMergeTest::~AutoFillMergeTest() {
}

void AutoFillMergeTest::SetUp() {
  autofill_test::DisableSystemServices(NULL);

  personal_data_ = new PersonalDataManagerMock();
}

void AutoFillMergeTest::GenerateResults(const std::string& input,
                                        std::string* output) {
  MergeProfiles(input, output);

  // Verify that the test is idempotent on the output profiles.
  std::string merged_output;
  MergeProfiles(*output, &merged_output);
  EXPECT_EQ(*output, merged_output);
}

void AutoFillMergeTest::MergeProfiles(const std::string& profiles,
                                      std::string* merged_profiles) {
  // Start with no saved profiles.
  personal_data_->Reset();

  // Create a test form.
  webkit_glue::FormData form;
  form.name = ASCIIToUTF16("MyTestForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("https://www.example.com/origin.html");
  form.action = GURL("https://www.example.com/action.html");
  form.user_submitted = true;

  // Parse the input line by line.
  std::vector<std::string> lines;
  Tokenize(profiles, "\n", &lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    std::string line = lines[i];

    if (line != kProfileSeparator) {
      // Add a field to the current profile.
      size_t separator_pos = line.find(kFieldSeparator);
      ASSERT_NE(std::string::npos, separator_pos);
      string16 field_type = UTF8ToUTF16(line.substr(0, separator_pos));
      string16 value = UTF8ToUTF16(line.substr(separator_pos + kFieldOffset));

      webkit_glue::FormField field(field_type,
                                   field_type,
                                   value,
                                   ASCIIToUTF16("text"),
                                   WebKit::WebInputElement::defaultMaxLength(),
                                   false);
      form.fields.push_back(field);
    }

    // The first line is always a profile separator, and the last profile is not
    // followed by an explicit separator.
    if ((i > 0 && line == kProfileSeparator) ||
        i == lines.size() - 1) {
      // Reached the end of a profile.  Try to import it.
      FormStructure form_structure(form);
      for (size_t i = 0; i < form_structure.field_count(); ++i) {
        // Set the heuristic type for each field, which is currently serialized
        // into the field's name.
        AutoFillField* field =
            const_cast<AutoFillField*>(form_structure.field(i));
        AutofillFieldType type =
            AutoFillType::StringToFieldType(UTF16ToUTF8(field->name()));
        field->set_heuristic_type(type);
      }
      std::vector<const FormStructure*> form_structures(1, &form_structure);

      // Import the profile.
      const CreditCard* imported_credit_card;
      personal_data_->ImportFormData(form_structures, &imported_credit_card);
      EXPECT_FALSE(imported_credit_card);

      // Clear the |form| to start a new profile.
      form.fields.clear();
    }
  }

  *merged_profiles = SerializeProfiles(personal_data_->web_profiles());
}

TEST_F(AutoFillMergeTest, DataDrivenMergeProfiles) {
  RunDataDrivenTest(GetInputDirectory(kTestName), GetOutputDirectory(kTestName),
                    kFileNamePattern);
}
