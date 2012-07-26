// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/browser/device_orientation/orientation.h"
#include "content/browser/device_orientation/provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"

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

class DeviceOrientationBrowserTest : public content::ContentBrowserTest {
 public:
  // From ContentBrowserTest.
  virtual void SetUpCommandLine(CommandLine* command_line) {
    EXPECT_TRUE(!command_line->HasSwitch(switches::kDisableDeviceOrientation));
  }
};

// crbug.com/113952
IN_PROC_BROWSER_TEST_F(DeviceOrientationBrowserTest, BasicTest) {
  Orientation test_orientation;
  test_orientation.set_alpha(1);
  test_orientation.set_beta(2);
  test_orientation.set_gamma(3);
  test_orientation.set_absolute(true);
  scoped_refptr<MockProvider> provider(new MockProvider(test_orientation));
  Provider::SetInstanceForTests(provider.get());

  // The test page will register an event handler for orientation events,
  // expects to get an event with kTestOrientation orientation,
  // then removes the event handler and navigates to #pass.
  GURL test_url = content::GetTestUrl(
      "device_orientation", "device_orientation_test.html");
  content::NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  // Check that the page got the event it expected and that the provider
  // saw requests for adding and removing an observer.
  EXPECT_EQ("pass", shell()->web_contents()->GetURL().ref());
  EXPECT_TRUE(provider->added_observer_);
  EXPECT_TRUE(provider->removed_observer_);
}

} //  namespace device_orientation
