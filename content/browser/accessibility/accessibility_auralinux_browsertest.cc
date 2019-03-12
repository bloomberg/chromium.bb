// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium cannot upgrade to ATK 2.12 API as it still needs to run
// valid builds for Ubuntu Trusty.
// TODO(accessibility): Remove this when Chromium drops support for ATK
// older than 2.12.
#define ATK_DISABLE_DEPRECATION_WARNINGS

#include <atk/atk.h>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/escape.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace content {

constexpr char kInputContents[] =
    "Moz/5.0 (ST 6.x; WWW33) "
    "WebKit  \"KHTML, like\".";
constexpr char kTextareaContents[] =
    "Moz/5.0 (ST 6.x; WWW33)\n"
    "WebKit \n\"KHTML, like\".";
constexpr int kContentsLength =
    static_cast<int>((sizeof(kInputContents) - 1) / sizeof(char));

class AccessibilityAuraLinuxBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityAuraLinuxBrowserTest() = default;
  ~AccessibilityAuraLinuxBrowserTest() override = default;

 protected:
  AtkObject* GetRendererAccessible() {
    content::WebContents* web_contents = shell()->web_contents();
    return web_contents->GetRenderWidgetHostView()->GetNativeViewAccessible();
  }

  static bool HasObjectWithAtkRoleFrameInAncestry(AtkObject* object) {
    while (object) {
      if (atk_object_get_role(object) == ATK_ROLE_FRAME)
        return true;
      object = atk_object_get_parent(object);
    }
    return false;
  }

  void LoadInitialAccessibilityTreeFromHtml(const std::string& html);
  static void CheckTextAtOffset(AtkText* text_object,
                                int offset,
                                AtkTextBoundary boundary_type,
                                int expected_start_offset,
                                int expected_end_offset,
                                const char* expected_text);

  void ExecuteScript(const std::string& script);
  AtkText* SetUpInputField();
  AtkText* SetUpTextareaField();
  AtkText* SetUpSampleParagraph();

  AtkText* GetAtkTextForChild(AtkRole expected_role);

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityAuraLinuxBrowserTest);
};

void AccessibilityAuraLinuxBrowserTest::ExecuteScript(
    const std::string& script) {
  shell()->web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script), base::NullCallback());
}

void AccessibilityAuraLinuxBrowserTest::LoadInitialAccessibilityTreeFromHtml(
    const std::string& html) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL html_data_url("data:text/html," +
                     net::EscapeQueryParamValue(html, false));
  NavigateToURL(shell(), html_data_url);
  waiter.WaitForNotification();
}

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
  LoadInitialAccessibilityTreeFromHtml(std::string(
                                           R"HTML(<!DOCTYPE html>
          <html>
          <body>
            <form>
              <label for="textField">Browser name:</label>
              <input type="text" id="textField" name="name" value=")HTML") +
                                       net::EscapeForHTML(kInputContents) +
                                       std::string(R"HTML(">
            </form>
          </body>
          </html>)HTML"));

  return GetAtkTextForChild(ATK_ROLE_ENTRY);
}

// Loads a page with  a textarea text field and places sample text in it. Also,
// places the caret before the last character.
AtkText* AccessibilityAuraLinuxBrowserTest::SetUpTextareaField() {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(<!DOCTYPE html>
      <html>
      <body>
                    <textarea rows="3" cols="60">)HTML") +
                                       net::EscapeForHTML(kTextareaContents) +
                                       std::string(R"HTML(</textarea>
          </body>
          </html>)HTML"));

  return GetAtkTextForChild(ATK_ROLE_ENTRY);
}

// Loads a page with  a paragraph of sample text.
AtkText* AccessibilityAuraLinuxBrowserTest::SetUpSampleParagraph() {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
          <p><b>Game theory</b> is "the study of
              <a href="" title="Mathematical model">mathematical models</a>
              of conflict and<br>cooperation between intelligent rational
              decision-makers."
          </p>
      </body>
      </html>)HTML");

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
                    kContentsLength, kInputContents);
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
                    kContentsLength, "\"KHTML, like\".");
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

}  // namespace content
