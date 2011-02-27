// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chromeos/cros/mock_mount_library.h"
#include "chrome/browser/chromeos/usb_mount_observer.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/mediaplayer_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/dom_element_proxy.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

class USBMountObserverBrowserTest : public InProcessBrowserTest {
 public:
  USBMountObserverBrowserTest() {}

  bool IsFilebrowserVisible() {
    for (BrowserList::const_iterator it = BrowserList::begin();
         it != BrowserList::end(); ++it) {
      if ((*it)->type() == Browser::TYPE_POPUP) {
        const GURL& url =
            (*it)->GetTabContentsAt((*it)->selected_index())->GetURL();
        if (url.SchemeIs(chrome::kChromeUIScheme) &&
            url.host() == chrome::kChromeUIFileBrowseHost) {
          return true;
        }
      }
    }
    return false;
  }
};

IN_PROC_BROWSER_TEST_F(USBMountObserverBrowserTest, PopupOnEvent) {
  StartHTTPServer();
  // Doing this so we have a valid profile
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeUIDownloadsURL));
  chromeos::USBMountObserver* observe = chromeos::USBMountObserver::GetInstance();
  observe->set_profile(browser()->profile());
  scoped_ptr<chromeos::MockMountLibrary> lib(new chromeos::MockMountLibrary());
  lib->AddObserver(observe);
  // Check that its not currently visible
  EXPECT_FALSE(IsFilebrowserVisible());

  lib->FireDeviceInsertEvents();

  EXPECT_TRUE(IsFilebrowserVisible());
}

}
