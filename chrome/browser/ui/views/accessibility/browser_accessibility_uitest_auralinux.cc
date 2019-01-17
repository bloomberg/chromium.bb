// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atk/atk.h>
#include <stddef.h>

#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"

class AuraLinuxAccessibilityInProcessBrowserTest : public InProcessBrowserTest {
 protected:
  AuraLinuxAccessibilityInProcessBrowserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AuraLinuxAccessibilityInProcessBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AuraLinuxAccessibilityInProcessBrowserTest,
                       IndexInParent) {
  AtkObject* native_view_accessible =
      static_cast<BrowserView*>(browser()->window())->GetNativeViewAccessible();
  EXPECT_NE(nullptr, native_view_accessible);

  int n_children = atk_object_get_n_accessible_children(native_view_accessible);
  for (int i = 0; i < n_children; i++) {
    AtkObject* child =
        atk_object_ref_accessible_child(native_view_accessible, i);

    int index_in_parent = atk_object_get_index_in_parent(child);
    ASSERT_NE(-1, index_in_parent);
    ASSERT_EQ(i, index_in_parent);

    g_object_unref(child);
  }
}
