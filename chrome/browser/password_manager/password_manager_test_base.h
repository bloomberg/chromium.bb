// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_BASE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_BASE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"

namespace autofill {
struct PasswordForm;
}

class ManagePasswordsUIController;

class NavigationObserver : public content::WebContentsObserver {
 public:
  explicit NavigationObserver(content::WebContents* web_contents);
  ~NavigationObserver() override;

  // Normally Wait() will not return until a main frame navigation occurs.
  // If a path is set, Wait() will return after this path has been seen,
  // regardless of the frame that navigated. Useful for multi-frame pages.
  void SetPathToWaitFor(const std::string& path) { wait_for_path_ = path; }

  // Normally Wait() will not return until a main frame navigation occurs.
  // If quit_on_entry_committed is true Wait() will return on EntryCommited.
  void set_quit_on_entry_committed(bool quit_on_entry_committed) {
    quit_on_entry_committed_ = quit_on_entry_committed;
  }

  // Wait for navigation to succeed.
  void Wait();

  // Returns the RenderFrameHost that navigated.
  content::RenderFrameHost* render_frame_host() { return render_frame_host_; }

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  std::string wait_for_path_;
  content::RenderFrameHost* render_frame_host_;
  bool quit_on_entry_committed_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(NavigationObserver);
};

// Observes the save password prompt for a specified WebContents, keeps track of
// whether or not it is currently shown, and allows accepting saving passwords
// through it.
class BubbleObserver {
 public:
  explicit BubbleObserver(content::WebContents* web_contents);

  // Checks if the save prompt is being currently shown.
  bool IsShowingSavePrompt() const;

  // Checks if the update prompt is being currently shown.
  bool IsShowingUpdatePrompt() const;

  // Dismisses the prompt currently open and moves the controller to the
  // inactive state.
  void Dismiss() const;

  // Expecting that the prompt is shown, saves the password. Checks that the
  // prompt is no longer visible afterwards.
  void AcceptSavePrompt() const;

  // Expecting that the prompt is shown, update |form| with the password from
  // observed form. Checks that the prompt is no longer visible afterwards.
  void AcceptUpdatePrompt(const autofill::PasswordForm& form) const;

 private:
  ManagePasswordsUIController* const passwords_ui_controller_;

  DISALLOW_COPY_AND_ASSIGN(BubbleObserver);
};

class PasswordManagerBrowserTestBase : public InProcessBrowserTest {
 public:
  PasswordManagerBrowserTestBase();
  ~PasswordManagerBrowserTestBase() override;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  // Wrapper around ui_test_utils::NavigateToURL that waits until
  // DidFinishLoad() fires. Normally this function returns after
  // DidStopLoading(), which caused flakiness as the NavigationObserver
  // would sometimes see the DidFinishLoad event from a previous navigation and
  // return immediately.
  void NavigateToFile(const std::string& path);

  // Navigates to |filename| and runs |submission_script| to submit. Navigates
  // back to |filename| and then verifies that |expected_element| has
  // |expected_value|.
  void VerifyPasswordIsSavedAndFilled(const std::string& filename,
                                      const std::string& submission_script,
                                      const std::string& expected_element,
                                      const std::string& expected_value);

  // Waits until the "value" attribute of the HTML element with |element_id| is
  // equal to |expected_value|. If the current value is not as expected, this
  // waits until the "change" event is fired for the element. This also
  // guarantees that once the real value matches the expected, the JavaScript
  // event loop is spun to allow all other possible events to take place.
  void WaitForElementValue(const std::string& element_id,
                           const std::string& expected_value);
  // Same as above except the element |element_id| is in iframe |iframe_id|
  void WaitForElementValue(const std::string& iframe_id,
                           const std::string& element_id,
                           const std::string& expected_value);
  // Make sure that the password store processed all the previous calls which
  // are executed on another thread.
  void WaitForPasswordStore();
  // Checks that the current "value" attribute of the HTML element with
  // |element_id| is equal to |expected_value|.
  void CheckElementValue(const std::string& element_id,
                         const std::string& expected_value);
  // Same as above except the element |element_id| is in iframe |iframe_id|
  void CheckElementValue(const std::string& iframe_id,
                         const std::string& element_id,
                         const std::string& expected_value);

  // Accessors
  content::WebContents* WebContents();
  content::RenderViewHost* RenderViewHost();

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerBrowserTestBase);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_BASE_H_
