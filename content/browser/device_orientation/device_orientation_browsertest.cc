// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/device_orientation/orientation.h"
#include "content/browser/device_orientation/provider.h"
#include "content/browser/tab_contents/tab_contents.h"

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

IN_PROC_BROWSER_TEST_F(DeviceOrientationBrowserTest, BasicTest) {
  const Orientation kTestOrientation(true, 1, true, 2, true, 3);
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
  EXPECT_EQ("pass", browser()->GetSelectedTabContents()->GetURL().ref());
  EXPECT_TRUE(provider->added_observer_);
  EXPECT_TRUE(provider->removed_observer_);
}

} //  namespace device_orientation
