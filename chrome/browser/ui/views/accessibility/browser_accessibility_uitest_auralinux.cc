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

namespace {

void WaitForStateChangeEvent(AtkObject* object) {
  base::RunLoop loop_runner;
  gulong handler_id =
      g_signal_connect(object, "state-change",
                       G_CALLBACK(+[](AtkObject*, gchar* /* state_changed */,
                                      gboolean /* new_value */,
                                      base::RunLoop* loop) { loop->Quit(); }),
                       &loop_runner);
  loop_runner.Run();
  g_signal_handler_disconnect(object, handler_id);
}

}  // namespace

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

static AtkObject* FindParentFrame(AtkObject* object) {
  while (object) {
    if (atk_object_get_role(object) == ATK_ROLE_FRAME)
      return object;
    object = atk_object_get_parent(object);
  }

  return nullptr;
}

IN_PROC_BROWSER_TEST_F(AuraLinuxAccessibilityInProcessBrowserTest,
                       EmbeddedRelationship) {
  AtkObject* native_view_accessible =
      static_cast<BrowserView*>(browser()->window())->GetNativeViewAccessible();
  EXPECT_NE(nullptr, native_view_accessible);

  AtkObject* window = FindParentFrame(native_view_accessible);
  EXPECT_NE(nullptr, window);

  AtkRelationSet* relations = atk_object_ref_relation_set(window);
  ASSERT_TRUE(atk_relation_set_contains(relations, ATK_RELATION_EMBEDS));

  AtkRelation* embeds_relation =
      atk_relation_set_get_relation_by_type(relations, ATK_RELATION_EMBEDS);
  EXPECT_NE(nullptr, embeds_relation);

  GPtrArray* targets = atk_relation_get_target(embeds_relation);
  EXPECT_NE(nullptr, targets);
  EXPECT_EQ(1u, targets->len);

  AtkObject* target = static_cast<AtkObject*>(g_ptr_array_index(targets, 0));
  EXPECT_NE(nullptr, target);

  g_object_unref(relations);

  relations = atk_object_ref_relation_set(target);
  ASSERT_TRUE(atk_relation_set_contains(relations, ATK_RELATION_EMBEDDED_BY));

  AtkRelation* embedded_by_relation = atk_relation_set_get_relation_by_type(
      relations, ATK_RELATION_EMBEDDED_BY);
  EXPECT_NE(nullptr, embedded_by_relation);

  targets = atk_relation_get_target(embedded_by_relation);
  EXPECT_NE(nullptr, targets);
  EXPECT_EQ(1u, targets->len);

  target = static_cast<AtkObject*>(g_ptr_array_index(targets, 0));
  ASSERT_EQ(target, window);

  g_object_unref(relations);
}

IN_PROC_BROWSER_TEST_F(AuraLinuxAccessibilityInProcessBrowserTest,
                       MinimizationState) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  AtkObject* frame = FindParentFrame(browser_view->GetNativeViewAccessible());
  CHECK(frame);

  AtkStateSet* state_set = atk_object_ref_state_set(frame);
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_ICONIFIED));
  g_object_unref(state_set);

  browser_view->Minimize();
  WaitForStateChangeEvent(frame);

  state_set = atk_object_ref_state_set(frame);
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_ICONIFIED));
  g_object_unref(state_set);

  // Restore and show both seem to be necessary to restore and remap the window
  // after minimization.
  browser_view->Restore();
  browser_view->Show();
  WaitForStateChangeEvent(frame);

  state_set = atk_object_ref_state_set(frame);
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_ICONIFIED));
  g_object_unref(state_set);
}
