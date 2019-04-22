// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atk/atk.h>
#include <dlfcn.h>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "content/browser/accessibility/accessibility_browsertest.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace content {

namespace {

AtkObject* FindAtkObjectParentFrame(AtkObject* atk_object) {
  while (atk_object) {
    if (atk_object_get_role(atk_object) == ATK_ROLE_FRAME)
      return atk_object;
    atk_object = atk_object_get_parent(atk_object);
  }
  return nullptr;
}

}  // namespace

class AccessibilityAuraLinuxBrowserTest : public AccessibilityBrowserTest {
 public:
  AccessibilityAuraLinuxBrowserTest() = default;
  ~AccessibilityAuraLinuxBrowserTest() override = default;

 protected:
  static bool HasObjectWithAtkRoleFrameInAncestry(AtkObject* object) {
    while (object) {
      if (atk_object_get_role(object) == ATK_ROLE_FRAME)
        return true;
      object = atk_object_get_parent(object);
    }
    return false;
  }

  static void CheckTextAtOffset(AtkText* text_object,
                                int offset,
                                AtkTextBoundary boundary_type,
                                int expected_start_offset,
                                int expected_end_offset,
                                const char* expected_text);

  AtkText* SetUpInputField();
  AtkText* SetUpTextareaField();
  AtkText* SetUpSampleParagraph();
  AtkText* SetUpSampleParagraphInScrollableDocument();

  AtkText* GetSampleParagraph();
  AtkText* GetAtkTextForChild(AtkRole expected_role);

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityAuraLinuxBrowserTest);
};

AtkText* AccessibilityAuraLinuxBrowserTest::GetAtkTextForChild(
    AtkRole expected_role) {
  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(1, atk_object_get_n_accessible_children(document));

  AtkObject* parent_element = atk_object_ref_accessible_child(document, 0);
  int number_of_children = atk_object_get_n_accessible_children(parent_element);
  EXPECT_LT(0, number_of_children);

  // The input field is always the last child.
  AtkObject* input =
      atk_object_ref_accessible_child(parent_element, number_of_children - 1);
  EXPECT_EQ(expected_role, atk_object_get_role(input));

  EXPECT_TRUE(ATK_IS_TEXT(input));
  AtkText* atk_text = ATK_TEXT(input);

  g_object_unref(parent_element);

  return atk_text;
}

// Loads a page with  an input text field and places sample text in it.
AtkText* AccessibilityAuraLinuxBrowserTest::SetUpInputField() {
  LoadInputField();
  return GetAtkTextForChild(ATK_ROLE_ENTRY);
}

// Loads a page with  a textarea text field and places sample text in it. Also,
// places the caret before the last character.
AtkText* AccessibilityAuraLinuxBrowserTest::SetUpTextareaField() {
  LoadTextareaField();
  return GetAtkTextForChild(ATK_ROLE_ENTRY);
}

// Loads a page with a paragraph of sample text.
AtkText* AccessibilityAuraLinuxBrowserTest::SetUpSampleParagraph() {
  LoadSampleParagraph();

  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(1, atk_object_get_n_accessible_children(document));

  int number_of_children = atk_object_get_n_accessible_children(document);
  EXPECT_LT(0, number_of_children);

  // The input field is always the last child.
  AtkObject* input = atk_object_ref_accessible_child(document, 0);
  EXPECT_EQ(ATK_ROLE_PARAGRAPH, atk_object_get_role(input));

  EXPECT_TRUE(ATK_IS_TEXT(input));
  return ATK_TEXT(input);
}

AtkText* AccessibilityAuraLinuxBrowserTest::GetSampleParagraph() {
  AtkObject* document = GetRendererAccessible();
  EXPECT_EQ(1, atk_object_get_n_accessible_children(document));

  int number_of_children = atk_object_get_n_accessible_children(document);
  EXPECT_LT(0, number_of_children);

  // The input field is always the last child.
  AtkObject* input = atk_object_ref_accessible_child(document, 0);
  EXPECT_EQ(ATK_ROLE_PARAGRAPH, atk_object_get_role(input));

  EXPECT_TRUE(ATK_IS_TEXT(input));
  return ATK_TEXT(input);
}

// Ensures that the text and the start and end offsets retrieved using
// get_textAtOffset match the expected values.
void AccessibilityAuraLinuxBrowserTest::CheckTextAtOffset(
    AtkText* text_object,
    int offset,
    AtkTextBoundary boundary_type,
    int expected_start_offset,
    int expected_end_offset,
    const char* expected_text) {
  testing::Message message;
  message << "While checking for \'" << expected_text << "\' at "
          << expected_start_offset << '-' << expected_end_offset << '.';
  SCOPED_TRACE(message);

  int start_offset = 0;
  int end_offset = 0;
  char* text = atk_text_get_text_at_offset(text_object, offset, boundary_type,
                                           &start_offset, &end_offset);
  EXPECT_EQ(expected_start_offset, start_offset);
  EXPECT_EQ(expected_end_offset, end_offset);
  EXPECT_STREQ(expected_text, text);
  g_free(text);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       AuraLinuxBrowserAccessibleParent) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  NavigateToURL(shell(), GURL("data:text/html,"));
  waiter.WaitForNotification();

  // Get the BrowserAccessibilityManager.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();
  ASSERT_NE(nullptr, manager);

  auto* host_view = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  ASSERT_NE(nullptr, host_view->GetNativeViewAccessible());

  AtkObject* host_view_parent =
      host_view->AccessibilityGetNativeViewAccessible();
  ASSERT_NE(nullptr, host_view_parent);
  ASSERT_TRUE(
      AccessibilityAuraLinuxBrowserTest::HasObjectWithAtkRoleFrameInAncestry(
          host_view_parent));
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestTextAtOffsetWithBoundaryLine) {
  AtkText* atk_text = SetUpInputField();

  // Single line text fields should return the whole text.
  CheckTextAtOffset(atk_text, 0, ATK_TEXT_BOUNDARY_LINE_START, 0,
                    InputContentsString().size(),
                    InputContentsString().c_str());
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestMultiLineTextAtOffsetWithBoundaryLine) {
  AtkText* atk_text = SetUpTextareaField();

  CheckTextAtOffset(atk_text, 0, ATK_TEXT_BOUNDARY_LINE_START, 0, 24,
                    "Moz/5.0 (ST 6.x; WWW33)\n");

  // If the offset is at the newline, return the line preceding it.
  CheckTextAtOffset(atk_text, 31, ATK_TEXT_BOUNDARY_LINE_START, 24, 32,
                    "WebKit \n");

  // Last line does not have a trailing newline.
  CheckTextAtOffset(atk_text, 32, ATK_TEXT_BOUNDARY_LINE_START, 32,
                    InputContentsString().size(), "\"KHTML, like\".");
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestParagraphTextAtOffsetWithBoundaryLine) {
  AtkText* atk_text = SetUpSampleParagraph();

  // There should be two lines in this paragraph.
  const int newline_offset = 46;
  int n_characters = atk_text_get_character_count(atk_text);
  ASSERT_LT(newline_offset, n_characters);

  const base::string16 string16_embed(
      1, ui::AXPlatformNodeAuraLinux::kEmbeddedCharacter);
  std::string expected_string = "Game theory is \"the study of " +
                                base::UTF16ToUTF8(string16_embed) +
                                " of conflict and\n";
  for (int i = 0; i <= newline_offset; ++i) {
    CheckTextAtOffset(atk_text, i, ATK_TEXT_BOUNDARY_LINE_START, 0,
                      newline_offset + 1, expected_string.c_str());
  }

  for (int i = newline_offset + 1; i < n_characters; ++i) {
    CheckTextAtOffset(
        atk_text, i, ATK_TEXT_BOUNDARY_LINE_START, newline_offset + 1,
        n_characters,
        "cooperation between intelligent rational decision-makers.\"");
  }
}

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 30, 0)
#define ATK_230
#endif

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestCharacterExtentsWithInvalidArguments) {
  AtkText* atk_text = SetUpSampleParagraph();

  int invalid_offset = -3;
  int x = -1, y = -1;
  int width = -1, height = -1;

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_SCREEN);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);

#ifdef ATK_230
  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_PARENT);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);
#endif  // ATK_230

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_WINDOW);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);

  int n_characters = atk_text_get_character_count(atk_text);
  ASSERT_LT(0, n_characters);

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_SCREEN);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);

#ifdef ATK_230
  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_PARENT);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);
#endif  // ATK_230

  atk_text_get_character_extents(atk_text, invalid_offset, &x, &y, &width,
                                 &height, ATK_XY_WINDOW);
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);
}

AtkCoordType kCoordinateTypes[] = {
    ATK_XY_SCREEN,
    ATK_XY_WINDOW,
#ifdef ATK_230
    ATK_XY_PARENT,
#endif  // ATK_230
};

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestCharacterExtentsInEditable) {
  AtkText* atk_text = SetUpSampleParagraph();

  constexpr int newline_offset = 46;
  int n_characters = atk_text_get_character_count(atk_text);
  ASSERT_EQ(105, n_characters);

  int x, y, width, height;
  int previous_x, previous_y, previous_height;
  for (AtkCoordType coordinate_type : kCoordinateTypes) {
    atk_text_get_character_extents(atk_text, 0, &x, &y, &width, &height,
                                   coordinate_type);
    EXPECT_LT(0, x) << "at offset 0";
    EXPECT_LT(0, y) << "at offset 0";
    EXPECT_LT(1, width) << "at offset 0";
    EXPECT_LT(1, height) << "at offset 0";

    gfx::Rect combined_extents(x, y, width, height);
    for (int offset = 1; offset < newline_offset; ++offset) {
      testing::Message message;
      message << "While checking at offset " << offset;
      SCOPED_TRACE(message);

      previous_x = x;
      previous_y = y;
      previous_height = height;

      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);
      EXPECT_LT(previous_x, x);
      EXPECT_EQ(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);

      combined_extents.Union(gfx::Rect(x, y, width, height));
      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);

      AtkTextRectangle atk_rect;
      atk_text_get_range_extents(atk_text, 0, offset + 1, coordinate_type,
                                 &atk_rect);
      EXPECT_EQ(combined_extents.x(), atk_rect.x);
      EXPECT_EQ(combined_extents.y(), atk_rect.y);
      EXPECT_EQ(combined_extents.width(), atk_rect.width);
      EXPECT_EQ(combined_extents.height(), atk_rect.height);
    }

    {
      testing::Message message;
      message << "While checking at offset " << newline_offset + 1;
      SCOPED_TRACE(message);

      atk_text_get_character_extents(atk_text, newline_offset + 1, &x, &y,
                                     &width, &height, coordinate_type);
      EXPECT_LE(0, x);
      EXPECT_GT(previous_x, x);
      EXPECT_LT(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);
    }

    combined_extents = gfx::Rect(x, y, width, height);
    for (int offset = newline_offset + 2; offset < n_characters; ++offset) {
      testing::Message message;
      message << "While checking at offset " << offset;
      SCOPED_TRACE(message);

      previous_x = x;
      previous_y = y;
      previous_height = height;

      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);
      EXPECT_LT(previous_x, x);
      EXPECT_EQ(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);

      combined_extents.Union(gfx::Rect(x, y, width, height));
      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);

      AtkTextRectangle atk_rect;
      atk_text_get_range_extents(atk_text, newline_offset + 1, offset + 1,
                                 coordinate_type, &atk_rect);
      EXPECT_EQ(combined_extents.x(), atk_rect.x);
      EXPECT_EQ(combined_extents.y(), atk_rect.y);
      EXPECT_EQ(combined_extents.width(), atk_rect.width);
      EXPECT_EQ(combined_extents.height(), atk_rect.height);
    }
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest,
                       TestCharacterExtentsInScrollableEditable) {
  LoadSampleParagraphInScrollableEditable();

  // By construction, only the first line of the content editable is visible.
  AtkText* atk_text = GetSampleParagraph();

  constexpr int first_line_end = 5;
  constexpr int last_line_start = 8;

  int n_characters = atk_text_get_character_count(atk_text);
  ASSERT_EQ(13, n_characters);

  int x, y, width, height;
  int previous_x, previous_y, previous_height;
  for (AtkCoordType coordinate_type : kCoordinateTypes) {
    // Test that non offscreen characters have increasing x coordinates and a
    // height that is greater than 1px.
    {
      testing::Message message;
      message << "While checking at offset 0";
      SCOPED_TRACE(message);

      atk_text_get_character_extents(atk_text, 0, &x, &y, &width, &height,
                                     coordinate_type);
      EXPECT_LT(0, x);
      EXPECT_LT(0, y);
      EXPECT_LT(1, width);
      EXPECT_LT(1, height);
    }

    for (int offset = 1; offset < first_line_end; ++offset) {
      testing::Message message;
      message << "While checking at offset " << offset;
      SCOPED_TRACE(message);

      previous_x = x;
      previous_y = y;
      previous_height = height;

      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);
      EXPECT_LT(previous_x, x);
      EXPECT_EQ(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);
    }

    {
      testing::Message message;
      message << "While checking at offset " << last_line_start;
      SCOPED_TRACE(message);

      atk_text_get_character_extents(atk_text, last_line_start, &x, &y, &width,
                                     &height, coordinate_type);
      EXPECT_LT(0, x);
      EXPECT_LT(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);
    }

    for (int offset = last_line_start + 1; offset < n_characters; ++offset) {
      testing::Message message;
      message << "While checking at offset " << offset;
      SCOPED_TRACE(message);

      previous_x = x;
      previous_y = y;

      atk_text_get_character_extents(atk_text, offset, &x, &y, &width, &height,
                                     coordinate_type);
      EXPECT_LT(previous_x, x);
      EXPECT_EQ(previous_y, y);
      EXPECT_LT(1, width);
      EXPECT_EQ(previous_height, height);
    }
  }
}

#if defined(ATK_230)
typedef bool (*ScrollToPointFunc)(AtkComponent* component,
                                  AtkCoordType coords,
                                  gint x,
                                  gint y);
typedef bool (*ScrollToFunc)(AtkComponent* component, AtkScrollType type);

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest, TestScrollToPoint) {
  // There's a chance we may be compiled with a newer version of ATK and then
  // run with an older one, so we need to do a runtime check for this method
  // that is available in ATK 2.30 instead of linking directly.
  ScrollToPointFunc scroll_to_point = reinterpret_cast<ScrollToPointFunc>(
      dlsym(RTLD_DEFAULT, "atk_component_scroll_to_point"));
  if (!scroll_to_point) {
    LOG(WARNING)
        << "Skipping AccessibilityAuraLinuxBrowserTest::TestScrollToPoint"
           " because ATK version < 2.30 detected.";
    return;
  }

  LoadSampleParagraphInScrollableDocument();
  AtkText* atk_text = GetSampleParagraph();
  ASSERT_TRUE(ATK_IS_COMPONENT(atk_text));
  AtkComponent* atk_component = ATK_COMPONENT(atk_text);

  int prev_x, prev_y, x, y;
  atk_component_get_extents(atk_component, &prev_x, &prev_y, nullptr, nullptr,
                            ATK_XY_SCREEN);

  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);
  scroll_to_point(atk_component, ATK_XY_PARENT, 0, 0);
  location_changed_waiter.WaitForNotification();

  atk_component_get_extents(atk_component, &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(prev_x, x);
  EXPECT_GT(prev_y, y);

  constexpr int kScrollToY = 0;
  scroll_to_point(atk_component, ATK_XY_SCREEN, 0, kScrollToY);
  location_changed_waiter.WaitForNotification();
  atk_component_get_extents(atk_component, &x, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(kScrollToY, y);

  constexpr int kScrollToY_2 = 243;
  scroll_to_point(atk_component, ATK_XY_SCREEN, 0, kScrollToY_2);
  location_changed_waiter.WaitForNotification();
  atk_component_get_extents(atk_component, nullptr, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);
  EXPECT_EQ(kScrollToY_2, y);

  scroll_to_point(atk_component, ATK_XY_SCREEN, 0, 129);
  location_changed_waiter.WaitForNotification();
  atk_component_get_extents(atk_component, nullptr, &y, nullptr, nullptr,
                            ATK_XY_SCREEN);

  AtkObject* frame = FindAtkObjectParentFrame(ATK_OBJECT(atk_component));
  int frame_y;
  atk_component_get_extents(ATK_COMPONENT(frame), nullptr, &frame_y, nullptr,
                            nullptr, ATK_XY_SCREEN);
  EXPECT_EQ(frame_y, y);
}

IN_PROC_BROWSER_TEST_F(AccessibilityAuraLinuxBrowserTest, TestScrollTo) {
  // There's a chance we may be compiled with a newer version of ATK and then
  // run with an older one, so we need to do a runtime check for this method
  // that is available in ATK 2.30 instead of linking directly.
  ScrollToFunc scroll_to = reinterpret_cast<ScrollToFunc>(
      dlsym(RTLD_DEFAULT, "atk_component_scroll_to"));
  if (!scroll_to) {
    LOG(WARNING) << "Skipping AccessibilityAuraLinuxBrowserTest::TestScrollTo"
                    " because ATK version < 2.30 detected.";
    return;
  }

  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div style="height: 5000px;"></div>
        <img src="" alt="Target1">
        <div style="height: 5000px;"></div>
        <img src="" alt="Target2">
        <div style="height: 5000px;"></div>
        <span>Target 3</span>
        <div style="height: 5000px;"></div>
      </body>
      </html>)HTML");

  // Retrieve the AtkObject interface for the document node.
  AtkObject* document = GetRendererAccessible();
  ASSERT_TRUE(ATK_IS_COMPONENT(document));

  // Get the dimensions of the document.
  int doc_x, doc_y, doc_width, doc_height;
  atk_component_get_extents(ATK_COMPONENT(document), &doc_x, &doc_y, &doc_width,
                            &doc_height, ATK_XY_SCREEN);

  // The document should only have two children, both with a role of GRAPHIC.
  ASSERT_EQ(3, atk_object_get_n_accessible_children(document));

  AtkObject* target = atk_object_ref_accessible_child(document, 0);
  AtkObject* target2 = atk_object_ref_accessible_child(document, 1);
  AtkObject* target3 = atk_object_ref_accessible_child(document, 2);

  ASSERT_TRUE(ATK_IS_COMPONENT(target));
  ASSERT_TRUE(ATK_IS_COMPONENT(target2));

  ASSERT_EQ(ATK_ROLE_IMAGE, atk_object_get_role(target));
  ASSERT_EQ(ATK_ROLE_IMAGE, atk_object_get_role(target2));

  // Call atk_component_scroll_to on the first target. Ensure it ends up very
  // near the center of the window.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kScrollPositionChanged);
  ASSERT_TRUE(scroll_to(ATK_COMPONENT(target), ATK_SCROLL_ANYWHERE));
  waiter.WaitForNotification();

  // Don't assume anything about the font size or the exact centering
  // behavior, just assert that the object is (roughly) centered by
  // checking that its top coordinate is between 40% and 60% of the
  // document's height.
  int x, y, width, height;
  atk_component_get_extents(ATK_COMPONENT(target), &x, &y, &width, &height,
                            ATK_XY_SCREEN);
  EXPECT_GT(y + height / 2, doc_y + 0.4 * doc_height);
  EXPECT_LT(y + height / 2, doc_y + 0.6 * doc_height);

  // Now call atk_component_scroll_to on the second target. Ensure it ends up
  // very near the center of the window.
  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kScrollPositionChanged);
  ASSERT_TRUE(scroll_to(ATK_COMPONENT(target2), ATK_SCROLL_ANYWHERE));
  waiter2.WaitForNotification();

  // Same as above, make sure it's roughly centered.
  atk_component_get_extents(ATK_COMPONENT(target2), &x, &y, &width, &height,
                            ATK_XY_SCREEN);
  EXPECT_GT(y + height / 2, doc_y + 0.4 * doc_height);
  EXPECT_LT(y + height / 2, doc_y + 0.6 * doc_height);

  // Orca expects atk_text_set_caret_offset to operate like scroll to the
  // target node like atk_component_scroll_to, so we test that here.
  ASSERT_TRUE(ATK_IS_TEXT(target3));
  AccessibilityNotificationWaiter waiter3(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kScrollPositionChanged);
  atk_text_set_caret_offset(ATK_TEXT(target3), 0);
  waiter3.WaitForNotification();

  // Same as above, make sure it's roughly centered.
  atk_component_get_extents(ATK_COMPONENT(target3), &x, &y, &width, &height,
                            ATK_XY_SCREEN);
  EXPECT_GT(y + height / 2, doc_y + 0.4 * doc_height);
  EXPECT_LT(y + height / 2, doc_y + 0.6 * doc_height);

  g_object_unref(target);
  g_object_unref(target2);
  g_object_unref(target3);
}
#endif  //  defined(ATK_230)

}  // namespace content
