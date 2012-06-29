// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/device_orientation/orientation.h"
#include "content/browser/device_orientation/provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"

namespace device_orientation {

class MockProvider : public Provider {
 public:
  explicit MockProvider(const Orientation& orientation)
      : orientation_(orientation),
        added_observer_(false),
        removed_observer_(false) {}

  virtual void AddObserver(Observer* observer) {
    added_observer_ = true;
    observer->OnOrientationUpdate(orientation_);
  }
  virtual void RemoveObserver(Observer* observer) {
    removed_observer_ = true;
  }

  Orientation orientation_;
  bool added_observer_;
  bool removed_observer_;

 private:
  virtual ~MockProvider() {}
};

class DeviceOrientationBrowserTest : public InProcessBrowserTest {
 public:
  // From InProcessBrowserTest.
  virtual void SetUpCommandLine(CommandLine* command_line) {
    EXPECT_TRUE(!command_line->HasSwitch(switches::kDisableDeviceOrientation));
  }

  GURL testUrl(const FilePath::CharType* filename) {
    const FilePath kTestDir(FILE_PATH_LITERAL("device_orientation"));
    return ui_test_utils::GetTestUrl(kTestDir, FilePath(filename));
  }
};

// crbug.com/113952
IN_PROC_BROWSER_TEST_F(DeviceOrientationBrowserTest, BasicTest) {
  const Orientation kTestOrientation(true, 1, true, 2, true, 3, true, true);
  scoped_refptr<MockProvider> provider(new MockProvider(kTestOrientation));
  Provider::SetInstanceForTests(provider.get());

  // The test page will register an event handler for orientation events,
  // expects to get an event with kTestOrientation orientation,
  // then removes the event handler and navigates to #pass.
  GURL test_url = testUrl(FILE_PATH_LITERAL("device_orientation_test.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            test_url,
                                                            2);

  // Check that the page got the event it expected and that the provider
  // saw requests for adding and removing an observer.
  EXPECT_EQ("pass", chrome::GetActiveWebContents(browser())->GetURL().ref());
  EXPECT_TRUE(provider->added_observer_);
  EXPECT_TRUE(provider->removed_observer_);
}

} //  namespace device_orientation
