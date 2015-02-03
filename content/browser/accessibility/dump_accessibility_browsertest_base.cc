// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"

#include <set>
#include <string>
#include <vector>

#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"

namespace content {

namespace {

const char kCommentToken = '#';
const char kMarkSkipFile[] = "#<skip";
const char kMarkEndOfFile[] = "<-- End-of-file -->";
const char kSignalDiff[] = "*";

}  // namespace

typedef AccessibilityTreeFormatter::Filter Filter;

DumpAccessibilityTestBase::DumpAccessibilityTestBase() {
}

DumpAccessibilityTestBase::~DumpAccessibilityTestBase() {
}

base::string16
DumpAccessibilityTestBase::DumpUnfilteredAccessibilityTreeAsString() {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  AccessibilityTreeFormatter formatter(
      web_contents->GetRootBrowserAccessibilityManager()->GetRoot());
  std::vector<Filter> filters;
  filters.push_back(Filter(base::ASCIIToUTF16("*"), Filter::ALLOW));
  formatter.SetFilters(filters);
  formatter.set_show_ids(true);
  base::string16 ax_tree_dump;
  formatter.FormatAccessibilityTree(&ax_tree_dump);
  return ax_tree_dump;
}

std::vector<int> DumpAccessibilityTestBase::DiffLines(
    const std::vector<std::string>& expected_lines,
    const std::vector<std::string>& actual_lines) {
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

void DumpAccessibilityTestBase::ParseHtmlForExtraDirectives(
    const std::string& test_html,
    std::vector<Filter>* filters,
    std::string* wait_for) {
  std::vector<std::string> lines;
  base::SplitString(test_html, '\n', &lines);
  for (std::vector<std::string>::const_iterator iter = lines.begin();
       iter != lines.end();
       ++iter) {
    const std::string& line = *iter;
    const std::string& allow_empty_str =
        AccessibilityTreeFormatter::GetAllowEmptyString();
    const std::string& allow_str =
        AccessibilityTreeFormatter::GetAllowString();
    const std::string& deny_str =
        AccessibilityTreeFormatter::GetDenyString();
    const std::string& wait_str = "@WAIT-FOR:";
    if (StartsWithASCII(line, allow_empty_str, true)) {
      filters->push_back(
          Filter(base::UTF8ToUTF16(line.substr(allow_empty_str.size())),
                 Filter::ALLOW_EMPTY));
    } else if (StartsWithASCII(line, allow_str, true)) {
      filters->push_back(Filter(base::UTF8ToUTF16(
          line.substr(allow_str.size())),
                                Filter::ALLOW));
    } else if (StartsWithASCII(line, deny_str, true)) {
      filters->push_back(Filter(base::UTF8ToUTF16(
          line.substr(deny_str.size())),
                                Filter::DENY));
    } else if (StartsWithASCII(line, wait_str, true)) {
      *wait_for = line.substr(wait_str.size());
    }
  }
}

void DumpAccessibilityTestBase::RunTest(
    const base::FilePath file_path, const char* file_dir) {
  // Disable the "hot tracked" state (set when the mouse is hovering over
  // an object) because it makes test output change based on the mouse position.
  BrowserAccessibilityStateImpl::GetInstance()->
      set_disable_hot_tracking_for_testing(true);

  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Output the test path to help anyone who encounters a failure and needs
  // to know where to look.
  printf("Testing: %s\n", file_path.MaybeAsASCII().c_str());

  std::string html_contents;
  base::ReadFileToString(file_path, &html_contents);

  // Read the expected file.
  std::string expected_contents_raw;
  base::FilePath expected_file =
    base::FilePath(file_path.RemoveExtension().value() +
                   AccessibilityTreeFormatter::GetExpectedFileSuffix());
  base::ReadFileToString(expected_file, &expected_contents_raw);

  // Tolerate Windows-style line endings (\r\n) in the expected file:
  // normalize by deleting all \r from the file (if any) to leave only \n.
  std::string expected_contents;
  base::RemoveChars(expected_contents_raw, "\r", &expected_contents);

  if (!expected_contents.compare(0, strlen(kMarkSkipFile), kMarkSkipFile)) {
    printf("Skipping this test on this platform.\n");
    return;
  }

  // Parse filters and other directives in the test file.
  std::string wait_for;
  AddDefaultFilters(&filters_);
  ParseHtmlForExtraDirectives(html_contents, &filters_, &wait_for);

  // Load the page.
  base::string16 html_contents16;
  html_contents16 = base::UTF8ToUTF16(html_contents);
  GURL url = GetTestUrl(file_dir, file_path.BaseName().MaybeAsASCII().c_str());

  // If there's a @WAIT-FOR directive, set up an accessibility notification
  // waiter that returns on any event; we'll stop when we get the text we're
  // waiting for, or time out. Otherwise just wait specifically for
  // the "load complete" event.
  scoped_ptr<AccessibilityNotificationWaiter> waiter;
  if (!wait_for.empty()) {
    waiter.reset(new AccessibilityNotificationWaiter(
        shell(), AccessibilityModeComplete, ui::AX_EVENT_NONE));
  } else {
    waiter.reset(new AccessibilityNotificationWaiter(
        shell(), AccessibilityModeComplete, ui::AX_EVENT_LOAD_COMPLETE));
  }

  // Load the test html.
  NavigateToURL(shell(), url);

  // Wait for notifications. If there's a @WAIT-FOR directive, break when
  // the text we're waiting for appears in the dump, otherwise break after
  // the first notification, which will be a load complete.
  do {
    waiter->WaitForNotification();
    if (!wait_for.empty()) {
      base::string16 tree_dump = DumpUnfilteredAccessibilityTreeAsString();
      if (base::UTF16ToUTF8(tree_dump).find(wait_for) != std::string::npos)
        wait_for.clear();
    }
  } while (!wait_for.empty());

  // Call the subclass to dump the output.
  std::vector<std::string> actual_lines = Dump();

  // Perform a diff (or write the initial baseline).
  std::vector<std::string> expected_lines;
  Tokenize(expected_contents, "\n", &expected_lines);
  // Marking the end of the file with a line of text ensures that
  // file length differences are found.
  expected_lines.push_back(kMarkEndOfFile);
  actual_lines.push_back(kMarkEndOfFile);
  std::string actual_contents = JoinString(actual_lines, "\n");

  std::vector<int> diff_lines = DiffLines(expected_lines, actual_lines);
  bool is_different = diff_lines.size() > 0;
  EXPECT_FALSE(is_different);
  if (is_different) {
    OnDiffFailed();

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
        ++diff_index;
      }
      printf("%1s %4d %s\n", is_diff? kSignalDiff : "", line + 1,
             expected_lines[line].c_str());
    }
    printf("\nActual\n");
    printf("------\n");
    printf("%s\n", actual_contents.c_str());
  }

  if (!base::PathExists(expected_file)) {
    base::FilePath actual_file =
        base::FilePath(file_path.RemoveExtension().value() +
                       AccessibilityTreeFormatter::GetActualFileSuffix());

    EXPECT_TRUE(base::WriteFile(
        actual_file, actual_contents.c_str(), actual_contents.size()));

    ADD_FAILURE() << "No expectation found. Create it by doing:\n"
                  << "mv " << actual_file.LossyDisplayName() << " "
                  << expected_file.LossyDisplayName();
  }
}

}  // namespace content
