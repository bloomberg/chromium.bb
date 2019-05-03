// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_event_recorder.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {

const char kCommentToken = '#';
const char kMarkSkipFile[] = "#<skip";
const char kMarkEndOfFile[] = "<-- End-of-file -->";
const char kSignalDiff[] = "*";

// Searches recursively and returns true if an accessibility node is found
// that represents a fully loaded web document with the given url.
bool AccessibilityTreeContainsLoadedDocWithUrl(BrowserAccessibility* node,
                                               const std::string& url) {
  if (node->GetRole() == ax::mojom::Role::kRootWebArea &&
      node->GetStringAttribute(ax::mojom::StringAttribute::kUrl) == url) {
    // Ensure the doc has finished loading and has a non-zero size.
    return node->manager()->GetTreeData().loaded &&
           (node->GetData().relative_bounds.bounds.width() > 0 &&
            node->GetData().relative_bounds.bounds.height() > 0);
  }
  if (node->GetRole() == ax::mojom::Role::kWebArea &&
      node->GetStringAttribute(ax::mojom::StringAttribute::kUrl) == url) {
    // Ensure the doc has finished loading.
    return node->manager()->GetTreeData().loaded;
  }

  for (unsigned i = 0; i < node->PlatformChildCount(); i++) {
    if (AccessibilityTreeContainsLoadedDocWithUrl(node->PlatformGetChild(i),
                                                  url)) {
      return true;
    }
  }
  return false;
}

}  // namespace

typedef AccessibilityTreeFormatter::PropertyFilter PropertyFilter;
typedef AccessibilityTreeFormatter::NodeFilter NodeFilter;

DumpAccessibilityTestBase::DumpAccessibilityTestBase()
    : formatter_factory_(nullptr),
      event_recorder_factory_(nullptr),
      enable_accessibility_after_navigating_(false) {}

DumpAccessibilityTestBase::~DumpAccessibilityTestBase() {}

void DumpAccessibilityTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  IsolateAllSitesForTesting(command_line);

  // Each test pass might require custom command-line setup
  auto passes = AccessibilityTreeFormatter::GetTestPasses();
  size_t current_pass = GetParam();
  CHECK_LT(current_pass, passes.size());
  if (passes[current_pass].set_up_command_line)
    passes[current_pass].set_up_command_line(command_line);
}

void DumpAccessibilityTestBase::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
}

void DumpAccessibilityTestBase::SetUp() {
  // TODO(dmazzoni): DumpAccessibilityTree expectations are based on the
  // assumption that the accessibility labels feature is off. (There are
  // also several tests that explicitly enable the feature.) It'd be better
  // if DumpAccessibilityTree tests assumed that the feature is on by
  // default instead.  http://crbug.com/940330
  scoped_feature_list_.InitAndDisableFeature(
      features::kExperimentalAccessibilityLabels);

  ContentBrowserTest::SetUp();
}

base::string16
DumpAccessibilityTestBase::DumpUnfilteredAccessibilityTreeAsString() {
  std::unique_ptr<AccessibilityTreeFormatter> formatter(formatter_factory_());
  std::vector<PropertyFilter> property_filters;
  property_filters.push_back(
      PropertyFilter(base::ASCIIToUTF16("*"), PropertyFilter::ALLOW));
  formatter->SetPropertyFilters(property_filters);
  formatter->set_show_ids(true);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  base::string16 ax_tree_dump;
  formatter->FormatAccessibilityTree(
      web_contents->GetRootBrowserAccessibilityManager()->GetRoot(),
      &ax_tree_dump);
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
    std::vector<std::string>* wait_for,
    std::vector<std::string>* run_until) {
  for (const std::string& line : base::SplitString(
           test_html, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    const std::string& allow_empty_str = formatter_->GetAllowEmptyString();
    const std::string& allow_str = formatter_->GetAllowString();
    const std::string& deny_str = formatter_->GetDenyString();
    const std::string& deny_node_str = formatter_->GetDenyNodeString();
    const std::string& wait_str = "@WAIT-FOR:";
    const std::string& until_str = "@RUN-UNTIL-EVENT:";
    if (base::StartsWith(line, allow_empty_str, base::CompareCase::SENSITIVE)) {
      property_filters_.push_back(
          PropertyFilter(base::UTF8ToUTF16(line.substr(allow_empty_str.size())),
                         PropertyFilter::ALLOW_EMPTY));
    } else if (base::StartsWith(line, allow_str,
                                base::CompareCase::SENSITIVE)) {
      property_filters_.push_back(
          PropertyFilter(base::UTF8ToUTF16(line.substr(allow_str.size())),
                         PropertyFilter::ALLOW));
    } else if (base::StartsWith(line, deny_str, base::CompareCase::SENSITIVE)) {
      property_filters_.push_back(
          PropertyFilter(base::UTF8ToUTF16(line.substr(deny_str.size())),
                         PropertyFilter::DENY));
    } else if (base::StartsWith(line, deny_node_str,
                                base::CompareCase::SENSITIVE)) {
      const auto& node_filter = line.substr(deny_node_str.size());
      const auto& parts = base::SplitString(
          node_filter, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      // Silently skip over parsing errors like the rest of the enclosing code.
      if (parts.size() == 2) {
        node_filters_.push_back(
            NodeFilter(parts[0], base::UTF8ToUTF16(parts[1])));
      }
    } else if (base::StartsWith(line, wait_str, base::CompareCase::SENSITIVE)) {
      wait_for->push_back(line.substr(wait_str.size()));
    } else if (base::StartsWith(line, until_str,
                                base::CompareCase::SENSITIVE)) {
      run_until->push_back(line.substr(until_str.size()));
    }
  }
}

void DumpAccessibilityTestBase::RunTest(const base::FilePath file_path,
                                        const char* file_dir) {
  // Get all the tree formatters; the test is run independently on each one.
  auto formatters = AccessibilityTreeFormatter::GetTestPasses();
  auto event_recorders = AccessibilityEventRecorder::GetTestPasses();
  CHECK(event_recorders.size() == formatters.size());

  // The current test number is supplied as a test parameter.
  size_t current_pass = GetParam();
  CHECK_LT(current_pass, formatters.size());
  CHECK_EQ(std::string(formatters[current_pass].name),
           std::string(event_recorders[current_pass].name));

  formatter_factory_ = formatters[current_pass].create_formatter;
  event_recorder_factory_ = event_recorders[current_pass].create_recorder;

  RunTestForPlatform(file_path, file_dir);

  formatter_factory_ = nullptr;
  event_recorder_factory_ = nullptr;
}

void DumpAccessibilityTestBase::RunTestForPlatform(
    const base::FilePath file_path,
    const char* file_dir) {
  formatter_ = formatter_factory_();

  // Disable the "hot tracked" state (set when the mouse is hovering over
  // an object) because it makes test output change based on the mouse position.
  BrowserAccessibilityStateImpl::GetInstance()
      ->set_disable_hot_tracking_for_testing(true);

  // Normally some accessibility events that would be fired are suppressed or
  // delayed, depending on what has focus or the type of event. For testing,
  // we want all events to fire immediately to make tests predictable and not
  // flaky.
  BrowserAccessibilityManager::NeverSuppressOrDelayEventsForTesting();

  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  bool path_exists = false;
  std::string html_contents;
  base::FilePath expected_file;
  std::string expected_contents_raw;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ReadFileToString(file_path, &html_contents);

    // Try to get version specific expected file.
    base::FilePath::StringType expected_file_suffix =
        formatter_->GetVersionSpecificExpectedFileSuffix();
    if (expected_file_suffix != FILE_PATH_LITERAL("")) {
      expected_file = base::FilePath(file_path.RemoveExtension().value() +
                                     expected_file_suffix);
      path_exists = base::PathExists(expected_file);
    }

    // If a version specific file does not exist, get the generic one.
    if (!path_exists) {
      expected_file_suffix = formatter_->GetExpectedFileSuffix();
      expected_file = base::FilePath(file_path.RemoveExtension().value() +
                                     expected_file_suffix);
      path_exists = base::PathExists(expected_file);
    }

    // If no expected file could be found, display error.
    if (!path_exists) {
      LOG(INFO) << "File not found: " << expected_file.LossyDisplayName();
      LOG(INFO)
          << "No expectation file present, ignoring test on this platform."
          << " To run this test anyway, create "
          << expected_file.LossyDisplayName()
          << " (it can be empty) and then run content_browsertests "
          << "with the switch: --"
          << switches::kGenerateAccessibilityTestExpectations;
      return;
    }
    base::ReadFileToString(expected_file, &expected_contents_raw);
  }

  // Output the test path to help anyone who encounters a failure and needs
  // to know where to look.
  LOG(INFO) << "Testing: "
            << file_path.NormalizePathSeparatorsTo('/').LossyDisplayName();
  LOG(INFO) << "Expected output: "
            << expected_file.NormalizePathSeparatorsTo('/').LossyDisplayName();

  // Tolerate Windows-style line endings (\r\n) in the expected file:
  // normalize by deleting all \r from the file (if any) to leave only \n.
  std::string expected_contents;
  base::RemoveChars(expected_contents_raw, "\r", &expected_contents);

  if (!expected_contents.compare(0, strlen(kMarkSkipFile), kMarkSkipFile)) {
    LOG(INFO) << "Skipping this test on this platform.";
    return;
  }

  // Parse filters and other directives in the test file.
  std::vector<std::string> wait_for;
  std::vector<std::string> run_until;
  property_filters_.clear();
  node_filters_.clear();
  formatter_->AddDefaultFilters(&property_filters_);
  AddDefaultFilters(&property_filters_);
  ParseHtmlForExtraDirectives(html_contents, &wait_for, &run_until);

  // Get the test URL.
  GURL url(embedded_test_server()->GetURL("/" + std::string(file_dir) + "/" +
                                          file_path.BaseName().MaybeAsASCII()));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  if (enable_accessibility_after_navigating_ &&
      web_contents->GetAccessibilityMode().is_mode_off()) {
    // Load the url, then enable accessibility.
    NavigateToURL(shell(), url);
    AccessibilityNotificationWaiter accessibility_waiter(
        web_contents, ui::kAXModeComplete, ax::mojom::Event::kNone);
    accessibility_waiter.WaitForNotification();
  } else {
    // Enable accessibility, then load the test html and wait for the
    // "load complete" AX event.
    AccessibilityNotificationWaiter accessibility_waiter(
        web_contents, ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
    NavigateToURL(shell(), url);
    accessibility_waiter.WaitForNotification();
  }

  // Get the url of every frame in the frame tree.
  FrameTree* frame_tree = web_contents->GetFrameTree();
  std::vector<std::string> all_frame_urls;
  for (FrameTreeNode* node : frame_tree->Nodes()) {
    // Ignore about:blank urls because of the case where a parent frame A
    // has a child iframe B and it writes to the document using
    // contentDocument.open() on the child frame B.
    //
    // In this scenario, B's contentWindow.location.href matches A's url,
    // but B's url in the browser frame tree is still "about:blank".
    std::string url = node->current_url().spec();
    if (url != url::kAboutBlankURL)
      all_frame_urls.push_back(url);
  }

  // Wait for the accessibility tree to fully load for all frames,
  // by searching for the WEB_AREA node in the accessibility tree
  // with the url of each frame in our frame tree. Note that this
  // doesn't support cases where there are two iframes with the
  // exact same url. If all frames haven't loaded yet, set up a
  // listener for accessibility events on any frame and block
  // until the next one is received.
  //
  // If the original page has a @WAIT-FOR directive, don't break until
  // the text we're waiting for appears in the full text dump of the
  // accessibility tree, either.
  for (;;) {
    VLOG(1) << "Top of loop";
    RenderFrameHostImpl* main_frame =
        static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
    BrowserAccessibilityManager* manager =
        main_frame->browser_accessibility_manager();
    if (manager) {
      BrowserAccessibility* accessibility_root = manager->GetRoot();

      // Check to see if all frames have loaded.
      bool all_frames_loaded = true;
      for (const auto& url : all_frame_urls) {
        if (!AccessibilityTreeContainsLoadedDocWithUrl(accessibility_root,
                                                       url)) {
          VLOG(1) << "Still waiting on this frame to load: " << url;
          all_frames_loaded = false;
          break;
        }
      }

      // Check to see if the @WAIT-FOR text has appeared yet.
      bool all_wait_for_strings_found = true;
      base::string16 tree_dump = DumpUnfilteredAccessibilityTreeAsString();
      for (const auto& str : wait_for) {
        if (base::UTF16ToUTF8(tree_dump).find(str) == std::string::npos) {
          VLOG(1) << "Still waiting on this text to be found: " << str;
          all_wait_for_strings_found = false;
          break;
        }
      }

      // If all frames have loaded and the @WAIT-FOR text has appeared,
      // we're done.
      if (all_frames_loaded && all_wait_for_strings_found)
        break;
    }

    // Block until the next accessibility notification in any frame.
    VLOG(1) << "Waiting until the next accessibility event";
    AccessibilityNotificationWaiter accessibility_waiter(
        web_contents, ui::AXMode(), ax::mojom::Event::kNone);
    accessibility_waiter.WaitForNotification();
  }

  // Call the subclass to dump the output.
  std::vector<std::string> actual_lines = Dump(run_until);
  std::string actual_contents_for_output =
      base::JoinString(actual_lines, "\n") + "\n";

  // Perform a diff (or write the initial baseline).
  std::vector<std::string> expected_lines =
      base::SplitString(expected_contents, "\n", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  // Marking the end of the file with a line of text ensures that
  // file length differences are found.
  expected_lines.push_back(kMarkEndOfFile);
  actual_lines.push_back(kMarkEndOfFile);
  std::string actual_contents = base::JoinString(actual_lines, "\n");

  std::vector<int> diff_lines = DiffLines(expected_lines, actual_lines);
  bool is_different = diff_lines.size() > 0;
  EXPECT_FALSE(is_different);
  if (is_different) {
    OnDiffFailed();

    std::string diff;

    // Mark the expected lines which did not match actual output with a *.
    diff += "* Line Expected\n";
    diff += "- ---- --------\n";
    for (int line = 0, diff_index = 0;
         line < static_cast<int>(expected_lines.size()); ++line) {
      bool is_diff = false;
      if (diff_index < static_cast<int>(diff_lines.size()) &&
          diff_lines[diff_index] == line) {
        is_diff = true;
        ++diff_index;
      }
      diff += base::StringPrintf("%1s %4d %s\n", is_diff ? kSignalDiff : "",
                                 line + 1, expected_lines[line].c_str());
    }
    diff += "\nActual\n";
    diff += "------\n";
    diff += actual_contents;
    LOG(ERROR) << "Diff:\n" << diff;
  } else {
    LOG(INFO) << "Test output matches expectations.";
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGenerateAccessibilityTestExpectations)) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::WriteFile(expected_file, actual_contents_for_output.c_str(),
                          actual_contents_for_output.size()) ==
          static_cast<int>(actual_contents_for_output.size()));
    LOG(INFO) << "Wrote expectations to: " << expected_file.LossyDisplayName();
  }
}

}  // namespace content
