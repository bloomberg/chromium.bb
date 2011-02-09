// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/data_driven_test.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"

namespace {

const FilePath::CharType kTestName[] = FILE_PATH_LITERAL("heuristics");
const FilePath::CharType kFileNamePattern[] = FILE_PATH_LITERAL("*.html");

// Convert the |html| snippet to a data URI.
GURL HTMLToDataURI(const std::string& html) {
  return GURL(std::string("data:text/html;charset=utf-8,") + html);
}

}  // namespace

// A data-driven test for verifying AutoFill heuristics. Each input is an HTML
// file that contains one or more forms. The corresponding output file lists the
// heuristically detected type for eachfield.
class FormStructureBrowserTest : public InProcessBrowserTest,
                                 public DataDrivenTest {
 protected:
  FormStructureBrowserTest();
  virtual ~FormStructureBrowserTest();

  // DataDrivenTest:
  virtual void GenerateResults(const std::string& input,
                               std::string* output);

  // Serializes the given |forms| into a string.
  std::string FormStructuresToString(const std::vector<FormStructure*>& forms);

 private:
  DISALLOW_COPY_AND_ASSIGN(FormStructureBrowserTest);
};

FormStructureBrowserTest::FormStructureBrowserTest() {
}

FormStructureBrowserTest::~FormStructureBrowserTest() {
}

void FormStructureBrowserTest::GenerateResults(const std::string& input,
                                               std::string* output) {
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
                                                       HTMLToDataURI(input)));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::ClickOnView(browser(),
                                                     VIEW_ID_TAB_CONTAINER));
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  AutoFillManager* autofill_manager =
      browser()->GetSelectedTabContents()->autofill_manager();
  ASSERT_NE(static_cast<AutoFillManager*>(NULL), autofill_manager);
  std::vector<FormStructure*> forms = autofill_manager->form_structures_.get();
  *output = FormStructureBrowserTest::FormStructuresToString(forms);
}

std::string FormStructureBrowserTest::FormStructuresToString(
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
      forms_string += AutoFillType::FieldTypeToString((*field_iter)->type());
      forms_string += "\n";
    }
  }
  return forms_string;
}

IN_PROC_BROWSER_TEST_F(FormStructureBrowserTest, DataDrivenHeuristics) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  RunDataDrivenTest(GetInputDirectory(kTestName),
                    GetOutputDirectory(kTestName),
                    kFileNamePattern);
}
