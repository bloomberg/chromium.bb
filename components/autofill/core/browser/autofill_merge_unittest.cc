// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_driven_test.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

namespace {

const base::FilePath::CharType kTestName[] = FILE_PATH_LITERAL("merge");
const base::FilePath::CharType kFileNamePattern[] = FILE_PATH_LITERAL("*.in");

const char kFieldSeparator[] = ": ";
const char kProfileSeparator[] = "---";
const size_t kFieldOffset = arraysize(kFieldSeparator) - 1;

const ServerFieldType kProfileFieldTypes[] = {
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
  PHONE_HOME_WHOLE_NUMBER
};

const base::FilePath& GetTestDataDir() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, dir, ());
  if (dir.empty()) {
    PathService::Get(base::DIR_SOURCE_ROOT, &dir);
    dir = dir.AppendASCII("components");
    dir = dir.AppendASCII("test");
    dir = dir.AppendASCII("data");
  }
  return dir;
}

// Serializes the |profiles| into a string.
std::string SerializeProfiles(const std::vector<AutofillProfile*>& profiles) {
  std::string result;
  for (size_t i = 0; i < profiles.size(); ++i) {
    result += kProfileSeparator;
    result += "\n";
    for (size_t j = 0; j < arraysize(kProfileFieldTypes); ++j) {
      ServerFieldType type = kProfileFieldTypes[j];
      std::vector<base::string16> values;
      profiles[i]->GetRawMultiInfo(type, &values);
      for (size_t k = 0; k < values.size(); ++k) {
        result += AutofillType(type).ToString();
        result += kFieldSeparator;
        result += base::UTF16ToUTF8(values[k]);
        result += "\n";
      }
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
  virtual std::string SaveImportedProfile(
      const AutofillProfile& profile) OVERRIDE;
  virtual const std::vector<AutofillProfile*>& web_profiles() const OVERRIDE;

 private:
  ScopedVector<AutofillProfile> profiles_;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManagerMock);
};

PersonalDataManagerMock::PersonalDataManagerMock()
    : PersonalDataManager("en-US") {
}

PersonalDataManagerMock::~PersonalDataManagerMock() {
}

void PersonalDataManagerMock::Reset() {
  profiles_.clear();
}

std::string PersonalDataManagerMock::SaveImportedProfile(
    const AutofillProfile& profile) {
  std::vector<AutofillProfile> profiles;
  std::string merged_guid =
      MergeProfile(profile, profiles_.get(), "en-US", &profiles);
  if (merged_guid == profile.guid())
    profiles_.push_back(new AutofillProfile(profile));
  return merged_guid;
}

const std::vector<AutofillProfile*>& PersonalDataManagerMock::web_profiles()
    const {
  return profiles_.get();
}

}  // namespace

// A data-driven test for verifying merging of Autofill profiles. Each input is
// a structured dump of a set of implicitly detected autofill profiles. The
// corresponding output file is a dump of the saved profiles that result from
// importing the input profiles. The output file format is identical to the
// input format.
class AutofillMergeTest : public testing::Test,
                          public DataDrivenTest {
 protected:
  AutofillMergeTest();
  virtual ~AutofillMergeTest();

  // testing::Test:
  virtual void SetUp();

  // DataDrivenTest:
  virtual void GenerateResults(const std::string& input,
                               std::string* output) OVERRIDE;

  // Deserializes a set of Autofill profiles from |profiles|, imports each
  // sequentially, and fills |merged_profiles| with the serialized result.
  void MergeProfiles(const std::string& profiles, std::string* merged_profiles);

  // Deserializes |str| into a field type.
  ServerFieldType StringToFieldType(const std::string& str);

  PersonalDataManagerMock personal_data_;

 private:
  std::map<std::string, ServerFieldType> string_to_field_type_map_;

  DISALLOW_COPY_AND_ASSIGN(AutofillMergeTest);
};

AutofillMergeTest::AutofillMergeTest() : DataDrivenTest(GetTestDataDir()) {
  for (size_t i = NO_SERVER_DATA; i < MAX_VALID_FIELD_TYPE; ++i) {
    ServerFieldType field_type = static_cast<ServerFieldType>(i);
    string_to_field_type_map_[AutofillType(field_type).ToString()] = field_type;
  }
}

AutofillMergeTest::~AutofillMergeTest() {
}

void AutofillMergeTest::SetUp() {
  test::DisableSystemServices(NULL);
}

void AutofillMergeTest::GenerateResults(const std::string& input,
                                        std::string* output) {
  MergeProfiles(input, output);
}

void AutofillMergeTest::MergeProfiles(const std::string& profiles,
                                      std::string* merged_profiles) {
  // Start with no saved profiles.
  personal_data_.Reset();

  // Create a test form.
  FormData form;
  form.name = base::ASCIIToUTF16("MyTestForm");
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
      base::string16 field_type =
          base::UTF8ToUTF16(line.substr(0, separator_pos));
      base::string16 value =
          base::UTF8ToUTF16(line.substr(separator_pos + kFieldOffset));

      FormFieldData field;
      field.label = field_type;
      field.name = field_type;
      field.value = value;
      field.form_control_type = "text";
      form.fields.push_back(field);
    }

    // The first line is always a profile separator, and the last profile is not
    // followed by an explicit separator.
    if ((i > 0 && line == kProfileSeparator) || i == lines.size() - 1) {
      // Reached the end of a profile.  Try to import it.
      FormStructure form_structure(form);
      for (size_t i = 0; i < form_structure.field_count(); ++i) {
        // Set the heuristic type for each field, which is currently serialized
        // into the field's name.
        AutofillField* field =
            const_cast<AutofillField*>(form_structure.field(i));
        ServerFieldType type =
            StringToFieldType(base::UTF16ToUTF8(field->name));
        field->set_heuristic_type(type);
      }

      // Import the profile.
      scoped_ptr<CreditCard> imported_credit_card;
      personal_data_.ImportFormData(form_structure, &imported_credit_card);
      EXPECT_EQ(static_cast<CreditCard*>(NULL), imported_credit_card.get());

      // Clear the |form| to start a new profile.
      form.fields.clear();
    }
  }

  *merged_profiles = SerializeProfiles(personal_data_.web_profiles());
}

ServerFieldType AutofillMergeTest::StringToFieldType(const std::string& str) {
  return string_to_field_type_map_[str];
}

TEST_F(AutofillMergeTest, DataDrivenMergeProfiles) {
  RunDataDrivenTest(GetInputDirectory(kTestName), GetOutputDirectory(kTestName),
                    kFileNamePattern);
}

}  // namespace autofill
