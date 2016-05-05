// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"

namespace content {

namespace {

const int kInvalidId = -1;

// The results of a find request.
struct FindResults {
  int request_id = kInvalidId;
  int number_of_matches = 0;
  int active_match_ordinal = 0;
};

}  // namespace

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate()
      : last_finished_request_id_(kInvalidId),
        waiting_request_id_(kInvalidId) {}
  ~TestWebContentsDelegate() override {}

  // Waits for the final reply to the find request with ID |request_id|.
  void WaitForFinalReply(int request_id) {
    if (last_finished_request_id_ >= request_id)
      return;

    waiting_request_id_ = request_id;
    find_message_loop_runner_ = new content::MessageLoopRunner;
    find_message_loop_runner_->Run();
  }

  // Returns the current find results.
  FindResults GetFindResults() {
    return current_results_;
  }

 private:
  // WebContentsDelegate override.
  void FindReply(WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override {
    // Update the current results.
    if (request_id > current_results_.request_id)
      current_results_.request_id = request_id;
    if (number_of_matches != -1)
      current_results_.number_of_matches = number_of_matches;
    if (active_match_ordinal != -1)
      current_results_.active_match_ordinal = active_match_ordinal;

    if (final_update)
      last_finished_request_id_ = request_id;

    // If we are waiting for a final reply and this is it, stop waiting.
    if (find_message_loop_runner_.get() &&
        last_finished_request_id_ >= waiting_request_id_) {
      find_message_loop_runner_->Quit();
    }
  }

  // The latest known results from the current find request.
  FindResults current_results_;

  // The ID of the last find request to finish (all replies received).
  int last_finished_request_id_;

  // If waiting using |find_message_loop_runner_|, this is the ID of the find
  // request being waited for.
  int waiting_request_id_;

  scoped_refptr<content::MessageLoopRunner> find_message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsDelegate);
};

class FindRequestManagerTest : public ContentBrowserTest {
 public:
  FindRequestManagerTest()
      : normal_delegate_(nullptr),
        last_request_id_(0) {}
  ~FindRequestManagerTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Swap the WebContents's delegate for our test delegate.
    normal_delegate_ = contents()->GetDelegate();
    contents()->SetDelegate(&test_delegate_);
  }

  void TearDownOnMainThread() override {
    // Swap the WebContents's delegate back to its usual delegate.
    contents()->SetDelegate(normal_delegate_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

 protected:
  // Navigate to |url| and wait for it to finish loading.
  void LoadAndWait(const std::string& url) {
    TestNavigationObserver navigation_observer(contents());
    NavigateToURL(shell(), embedded_test_server()->GetURL("a.com", url));
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  void Find(const std::string& search_text,
            const blink::WebFindOptions& options) {
    contents()->Find(++last_request_id_,
                     base::UTF8ToUTF16(search_text),
                     options);
  }

  void WaitForFinalReply() const {
    delegate()->WaitForFinalReply(last_request_id_);
  }

  WebContents* contents() const {
    return shell()->web_contents();
  }

  TestWebContentsDelegate* delegate() const {
    return static_cast<TestWebContentsDelegate*>(contents()->GetDelegate());
  }

  int last_request_id() const {
    return last_request_id_;
  }

 private:
  TestWebContentsDelegate test_delegate_;
  WebContentsDelegate* normal_delegate_;

  // The ID of the last find request requested.
  int last_request_id_;

  DISALLOW_COPY_AND_ASSIGN(FindRequestManagerTest);
};

// TODO(paulmeyer): These tests currently fail on the linux_android_rel_ng
// trybot. Remove this guard once that problem is figured out.
#if !defined(OS_ANDROID)

// Test basic find-in-page functionality (such as searching forward and
// backward) and check for correct results at each step.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, Basic) {
  LoadAndWait("/find_in_page.html");

  blink::WebFindOptions options;
  Find("result", options);
  WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);

  options.findNext = true;
  for (int i = 2; i <= 10; ++i) {
    Find("result", options);
    WaitForFinalReply();

    results = delegate()->GetFindResults();
    EXPECT_EQ(last_request_id(), results.request_id);
    EXPECT_EQ(19, results.number_of_matches);
    EXPECT_EQ(i, results.active_match_ordinal);
  }

  options.forward = false;
  for (int i = 9; i >= 5; --i) {
    Find("result", options);
    WaitForFinalReply();

    results = delegate()->GetFindResults();
    EXPECT_EQ(last_request_id(), results.request_id);
    EXPECT_EQ(19, results.number_of_matches);
    EXPECT_EQ(i, results.active_match_ordinal);
  }
}

// Tests searching for a word character-by-character, as would typically be done
// by a user typing into the find bar.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, CharacterByCharacter) {
  LoadAndWait("/find_in_page.html");

  blink::WebFindOptions default_options;
  Find("r", default_options);
  Find("re", default_options);
  Find("res", default_options);
  Find("resu", default_options);
  Find("resul", default_options);
  Find("result", default_options);
  WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(1, results.active_match_ordinal);
}

// Test sending a large number of find requests subsequently.
IN_PROC_BROWSER_TEST_F(FindRequestManagerTest, RapidFire) {
  LoadAndWait("/find_in_page.html");

  blink::WebFindOptions options;
  Find("result", options);

  options.findNext = true;
  for (int i = 2; i <= 1000; ++i)
    Find("result", options);
  WaitForFinalReply();

  FindResults results = delegate()->GetFindResults();
  EXPECT_EQ(last_request_id(), results.request_id);
  EXPECT_EQ(19, results.number_of_matches);
  EXPECT_EQ(last_request_id() % results.number_of_matches,
            results.active_match_ordinal);
}

#endif  // OS_ANDROID

}  // namespace content
