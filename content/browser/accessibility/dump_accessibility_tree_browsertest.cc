// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/dump_accessibility_tree_helper.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(dmazzoni): Disabled accessibility tests on Win64. crbug.com/179717
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

namespace content {

namespace {

const char kCommentToken = '#';
const char kMarkSkipFile[] = "#<skip";
const char kMarkEndOfFile[] = "<-- End-of-file -->";
const char kSignalDiff[] = "*";

}  // namespace

typedef DumpAccessibilityTreeHelper::Filter Filter;

// This test takes a snapshot of the platform BrowserAccessibility tree and
// tests it against an expected baseline.
//
// The flow of the test is as outlined below.
// 1. Load an html file from chrome/test/data/accessibility.
// 2. Read the expectation.
// 3. Browse to the page and serialize the platform specific tree into a human
//    readable string.
// 4. Perform a comparison between actual and expected and fail if they do not
//    exactly match.
class DumpAccessibilityTreeTest : public ContentBrowserTest {
 public:
  // Utility helper that does a comment aware equality check.
  // Returns array of lines from expected file which are different.
  std::vector<int> DiffLines(std::vector<std::string>& expected_lines,
                             std::vector<std::string>& actual_lines) {
    int actual_lines_count = actual_lines.size();
    int expected_lines_count = expected_lines.size();
    std::vector<int> diff_lines;
    int i = 0, j = 0;
    while (i < actual_lines_count && j < expected_lines_count) {
      if (expected_lines[j].size() == 0 ||
          expected_lines[j][0] == kCommentToken) {
        // Skip comment lines and blank lines in expected output.
        ++j;
        continue;
      }

      if (actual_lines[i] != expected_lines[j])
        diff_lines.push_back(j);
      ++i;
      ++j;
    }

    // Actual file has been fully checked.
    return diff_lines;
  }

  void AddDefaultFilters(std::vector<Filter>* filters) {
    filters->push_back(Filter(ASCIIToUTF16("FOCUSABLE"), Filter::ALLOW));
    filters->push_back(Filter(ASCIIToUTF16("READONLY"), Filter::ALLOW));
    filters->push_back(Filter(ASCIIToUTF16("*=''"), Filter::DENY));
  }

  void ParseFilters(const std::string& test_html,
                    std::vector<Filter>* filters) {
    std::vector<std::string> lines;
    base::SplitString(test_html, '\n', &lines);
    for (std::vector<std::string>::const_iterator iter = lines.begin();
         iter != lines.end();
         ++iter) {
      const std::string& line = *iter;
      const std::string& allow_empty_str = helper_.GetAllowEmptyString();
      const std::string& allow_str = helper_.GetAllowString();
      const std::string& deny_str = helper_.GetDenyString();
      if (StartsWithASCII(line, allow_empty_str, true)) {
        filters->push_back(
          Filter(UTF8ToUTF16(line.substr(allow_empty_str.size())),
                 Filter::ALLOW_EMPTY));
      } else if (StartsWithASCII(line, allow_str, true)) {
        filters->push_back(Filter(UTF8ToUTF16(line.substr(allow_str.size())),
                                  Filter::ALLOW));
      } else if (StartsWithASCII(line, deny_str, true)) {
        filters->push_back(Filter(UTF8ToUTF16(line.substr(deny_str.size())),
                                  Filter::DENY));
      }
    }
  }

  void RunTest(const base::FilePath::CharType* file_path);

  DumpAccessibilityTreeHelper helper_;
};

void DumpAccessibilityTreeTest::RunTest(
    const base::FilePath::CharType* file_path) {
  NavigateToURL(shell(), GURL("about:blank"));
  RenderWidgetHostViewPort* host_view = static_cast<RenderWidgetHostViewPort*>(
      shell()->web_contents()->GetRenderWidgetHostView());
  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(host_view->GetRenderWidgetHost());
  RenderViewHostImpl* view_host = static_cast<RenderViewHostImpl*>(host);
  view_host->set_save_accessibility_tree_for_testing(true);
  view_host->SetAccessibilityMode(AccessibilityModeComplete);

  // Setup test paths.
  base::FilePath dir_test_data;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &dir_test_data));
  base::FilePath test_path(
      dir_test_data.Append(FILE_PATH_LITERAL("accessibility")));
  ASSERT_TRUE(file_util::PathExists(test_path))
      << test_path.LossyDisplayName();

  base::FilePath html_file = test_path.Append(base::FilePath(file_path));
  // Output the test path to help anyone who encounters a failure and needs
  // to know where to look.
  printf("Testing: %s\n", html_file.MaybeAsASCII().c_str());

  std::string html_contents;
  file_util::ReadFileToString(html_file, &html_contents);

  // Parse filters in the test file.
  std::vector<Filter> filters;
  AddDefaultFilters(&filters);
  ParseFilters(html_contents, &filters);
  helper_.SetFilters(filters);

  // Read the expected file.
  std::string expected_contents_raw;
  base::FilePath expected_file =
    base::FilePath(html_file.RemoveExtension().value() +
             helper_.GetExpectedFileSuffix());
  file_util::ReadFileToString(
      expected_file,
      &expected_contents_raw);

  // Tolerate Windows-style line endings (\r\n) in the expected file:
  // normalize by deleting all \r from the file (if any) to leave only \n.
  std::string expected_contents;
  RemoveChars(expected_contents_raw, "\r", &expected_contents);

  if (!expected_contents.compare(0, strlen(kMarkSkipFile), kMarkSkipFile)) {
    printf("Skipping this test on this platform.\n");
    return;
  }

  // Load the page.
  string16 html_contents16;
  html_contents16 = UTF8ToUTF16(html_contents);
  GURL url = GetTestUrl("accessibility",
                        html_file.BaseName().MaybeAsASCII().c_str());
  scoped_refptr<MessageLoopRunner> loop_runner(new MessageLoopRunner);
  view_host->SetAccessibilityLoadCompleteCallbackForTesting(
      loop_runner->QuitClosure());
  NavigateToURL(shell(), url);

  // Wait for the tree.
  loop_runner->Run();

  // Perform a diff (or write the initial baseline).
  string16 actual_contents_utf16;
  helper_.DumpAccessibilityTree(
      host_view->GetBrowserAccessibilityManager()->GetRoot(),
      &actual_contents_utf16);
  std::string actual_contents = UTF16ToUTF8(actual_contents_utf16);
  std::vector<std::string> actual_lines, expected_lines;
  Tokenize(actual_contents, "\n", &actual_lines);
  Tokenize(expected_contents, "\n", &expected_lines);
  // Marking the end of the file with a line of text ensures that
  // file length differences are found.
  expected_lines.push_back(kMarkEndOfFile);
  actual_lines.push_back(kMarkEndOfFile);

  std::vector<int> diff_lines = DiffLines(expected_lines, actual_lines);
  bool is_different = diff_lines.size() > 0;
  EXPECT_FALSE(is_different);
  if (is_different) {
    // Mark the expected lines which did not match actual output with a *.
    printf("* Line Expected\n");
    printf("- ---- --------\n");
    for (int line = 0, diff_index = 0;
         line < static_cast<int>(expected_lines.size());
         ++line) {
      bool is_diff = false;
      if (diff_index < static_cast<int>(diff_lines.size()) &&
          diff_lines[diff_index] == line) {
        is_diff = true;
        ++ diff_index;
      }
      printf("%1s %4d %s\n", is_diff? kSignalDiff : "", line + 1,
             expected_lines[line].c_str());
    }
    printf("\nActual\n");
    printf("------\n");
    printf("%s\n", actual_contents.c_str());
  }

  if (!file_util::PathExists(expected_file)) {
    base::FilePath actual_file =
        base::FilePath(html_file.RemoveExtension().value() +
                       helper_.GetActualFileSuffix());

    EXPECT_TRUE(file_util::WriteFile(
        actual_file, actual_contents.c_str(), actual_contents.size()));

    ADD_FAILURE() << "No expectation found. Create it by doing:\n"
                  << "mv " << actual_file.LossyDisplayName() << " "
                  << expected_file.LossyDisplayName();
  }
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityA) {
  RunTest(FILE_PATH_LITERAL("a.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAddress) {
  RunTest(FILE_PATH_LITERAL("address.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAName) {
  RunTest(FILE_PATH_LITERAL("a-name.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAOnclick) {
  RunTest(FILE_PATH_LITERAL("a-onclick.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaApplication) {
  RunTest(FILE_PATH_LITERAL("aria-application.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaAutocomplete) {
  RunTest(FILE_PATH_LITERAL("aria-autocomplete.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaCombobox) {
  RunTest(FILE_PATH_LITERAL("aria-combobox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaInvalid) {
  RunTest(FILE_PATH_LITERAL("aria-invalid.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       MAYBE(AccessibilityAriaLevel)) {
  RunTest(FILE_PATH_LITERAL("aria-level.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMenu) {
  RunTest(FILE_PATH_LITERAL("aria-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaMenuitemradio) {
  RunTest(FILE_PATH_LITERAL("aria-menuitemradio.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaPressed) {
  RunTest(FILE_PATH_LITERAL("aria-pressed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaProgressbar) {
  RunTest(FILE_PATH_LITERAL("aria-progressbar.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaToolbar) {
  RunTest(FILE_PATH_LITERAL("toolbar.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaValueMin) {
  RunTest(FILE_PATH_LITERAL("aria-valuemin.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaValueMax) {
  RunTest(FILE_PATH_LITERAL("aria-valuemax.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityArticle) {
  RunTest(FILE_PATH_LITERAL("article.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAWithImg) {
  RunTest(FILE_PATH_LITERAL("a-with-img.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBdo) {
  RunTest(FILE_PATH_LITERAL("bdo.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBR) {
  RunTest(FILE_PATH_LITERAL("br.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityButtonNameCalc) {
  RunTest(FILE_PATH_LITERAL("button-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCanvas) {
  RunTest(FILE_PATH_LITERAL("canvas.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityCheckboxNameCalc) {
  RunTest(FILE_PATH_LITERAL("checkbox-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDiv) {
  RunTest(FILE_PATH_LITERAL("div.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDl) {
  RunTest(FILE_PATH_LITERAL("dl.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableDescendants) {
  RunTest(FILE_PATH_LITERAL("contenteditable-descendants.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityEm) {
  RunTest(FILE_PATH_LITERAL("em.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFooter) {
  RunTest(FILE_PATH_LITERAL("footer.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityForm) {
  RunTest(FILE_PATH_LITERAL("form.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHeading) {
  RunTest(FILE_PATH_LITERAL("heading.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHR) {
  RunTest(FILE_PATH_LITERAL("hr.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       MAYBE(AccessibilityIframeCoordinates)) {
  RunTest(FILE_PATH_LITERAL("iframe-coordinates.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputButton) {
  RunTest(FILE_PATH_LITERAL("input-button.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputButtonInMenu) {
  RunTest(FILE_PATH_LITERAL("input-button-in-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputColor) {
  RunTest(FILE_PATH_LITERAL("input-color.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputImageButtonInMenu) {
  RunTest(FILE_PATH_LITERAL("input-image-button-in-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputRange) {
  RunTest(FILE_PATH_LITERAL("input-range.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputTextNameCalc) {
  RunTest(FILE_PATH_LITERAL("input-text-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLabel) {
  RunTest(FILE_PATH_LITERAL("label.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityListMarkers) {
  RunTest(FILE_PATH_LITERAL("list-markers.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityP) {
  RunTest(FILE_PATH_LITERAL("p.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySelect) {
  RunTest(FILE_PATH_LITERAL("select.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySpan) {
  RunTest(FILE_PATH_LITERAL("span.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySpinButton) {
  RunTest(FILE_PATH_LITERAL("spinbutton.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySvg) {
  RunTest(FILE_PATH_LITERAL("svg.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTab) {
  RunTest(FILE_PATH_LITERAL("tab.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityToggleButton) {
  RunTest(FILE_PATH_LITERAL("togglebutton.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityUl) {
  RunTest(FILE_PATH_LITERAL("ul.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityWbr) {
  RunTest(FILE_PATH_LITERAL("wbr.html"));
}

}  // namespace content
