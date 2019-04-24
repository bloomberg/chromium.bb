// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_com_win.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

using Microsoft::WRL::ComPtr;

namespace content {

#define EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(safearray, expected_property_values) \
  {                                                                         \
    EXPECT_EQ(sizeof(V_R8(LPVARIANT(NULL))),                                \
              ::SafeArrayGetElemsize(safearray));                           \
    ASSERT_EQ(1u, SafeArrayGetDim(safearray));                              \
    LONG array_lower_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetLBound(safearray, 1, &array_lower_bound));              \
    LONG array_upper_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetUBound(safearray, 1, &array_upper_bound));              \
    double* array_data;                                                     \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(                         \
        safearray, reinterpret_cast<void**>(&array_data)));                 \
    size_t count = array_upper_bound - array_lower_bound + 1;               \
    ASSERT_EQ(expected_property_values.size(), count);                      \
    for (size_t i = 0; i < count; ++i) {                                    \
      EXPECT_EQ(array_data[i], expected_property_values[i]);                \
    }                                                                       \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(safearray));           \
  }

#define EXPECT_UIA_TEXTRANGE_EQ(provider, expected_content) \
  {                                                         \
    base::win::ScopedBstr provider_content;                 \
    ASSERT_HRESULT_SUCCEEDED(                               \
        provider->GetText(-1, provider_content.Receive())); \
    EXPECT_STREQ(expected_content, provider_content);       \
  }

class AXPlatformNodeTextRangeProviderWinBrowserTest
    : public ContentBrowserTest {
 protected:
  void LoadInitialAccessibilityTreeFromUrl(
      const GURL& url,
      ui::AXMode accessibility_mode = ui::kAXModeComplete) {
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           accessibility_mode,
                                           ax::mojom::Event::kLoadComplete);
    NavigateToURL(shell(), url);
    waiter.WaitForNotification();
  }

  void LoadInitialAccessibilityTreeFromHtmlFilePath(
      const std::string& html_file_path,
      ui::AXMode accessibility_mode = ui::kAXModeComplete) {
    if (!embedded_test_server()->Started())
      ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_test_server()->Started());
    LoadInitialAccessibilityTreeFromUrl(
        embedded_test_server()->GetURL(html_file_path), accessibility_mode);
  }

  void LoadInitialAccessibilityTreeFromHtml(
      const std::string& html,
      ui::AXMode accessibility_mode = ui::kAXModeComplete) {
    LoadInitialAccessibilityTreeFromUrl(
        GURL("data:text/html," + net::EscapeQueryParamValue(html, false)),
        accessibility_mode);
  }

  BrowserAccessibilityManager* GetManagerAndAssertNonNull() {
    auto GetManagerAndAssertNonNull =
        [this](BrowserAccessibilityManager** result) {
          WebContentsImpl* web_contents_impl =
              static_cast<WebContentsImpl*>(shell()->web_contents());
          ASSERT_NE(nullptr, web_contents_impl);
          BrowserAccessibilityManager* browser_accessibility_manager =
              web_contents_impl->GetRootBrowserAccessibilityManager();
          ASSERT_NE(nullptr, browser_accessibility_manager);
          *result = browser_accessibility_manager;
        };

    BrowserAccessibilityManager* browser_accessibility_manager;
    GetManagerAndAssertNonNull(&browser_accessibility_manager);
    return browser_accessibility_manager;
  }

  BrowserAccessibility* GetRootAndAssertNonNull() {
    auto GetRootAndAssertNonNull = [this](BrowserAccessibility** result) {
      BrowserAccessibility* root_browser_accessibility =
          GetManagerAndAssertNonNull()->GetRoot();
      ASSERT_NE(nullptr, result);
      *result = root_browser_accessibility;
    };

    BrowserAccessibility* root_browser_accessibility;
    GetRootAndAssertNonNull(&root_browser_accessibility);
    return root_browser_accessibility;
  }

  BrowserAccessibility* FindNode(ax::mojom::Role role,
                                 const std::string& name_or_value) {
    return FindNodeInSubtree(*GetRootAndAssertNonNull(), role, name_or_value);
  }

  void GetTextRangeProviderFromTextNode(
      ComPtr<ITextRangeProvider>& text_range_provider,
      BrowserAccessibility* target_browser_accessibility) {
    auto* provider_simple =
        ToBrowserAccessibilityWin(target_browser_accessibility)->GetCOM();
    ASSERT_NE(nullptr, provider_simple);

    ComPtr<ITextProvider> text_provider;
    EXPECT_HRESULT_SUCCEEDED(
        provider_simple->GetPatternProvider(UIA_TextPatternId, &text_provider));
    ASSERT_NE(nullptr, text_provider.Get());

    EXPECT_HRESULT_SUCCEEDED(
        text_provider->get_DocumentRange(&text_range_provider));
    ASSERT_NE(nullptr, text_range_provider.Get());
  }

 private:
  BrowserAccessibility* FindNodeInSubtree(BrowserAccessibility& node,
                                          ax::mojom::Role role,
                                          const std::string& name_or_value) {
    const auto& name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    const auto& value =
        node.GetStringAttribute(ax::mojom::StringAttribute::kValue);
    if (node.GetRole() == role &&
        (name == name_or_value || value == name_or_value)) {
      return &node;
    }

    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      BrowserAccessibility* result =
          FindNodeInSubtree(*node.PlatformGetChild(i), role, name_or_value);
      if (result)
        return result;
    }

    return nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetBoundingRectangles) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .break_word {
              width: 50px;
              word-wrap: break-word;
            }
          </style>
        </head>
        <body>
          <p class="break_word">AsdfAsdfAsdf</p>
        </body>
      </html>
  )HTML"));

  auto* node = FindNode(ax::mojom::Role::kStaticText, "AsdfAsdfAsdf");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider, node);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"AsdfAsdfAsdf");

  base::win::ScopedSafearray rectangles;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));

  // |view_offset| is necessary to account for differences in the shell
  // between platforms (e.g. title bar height) because the results of
  // |GetBoundingRectangles| are in screen coordinates.
  gfx::Vector2d view_offset =
      node->manager()->GetViewBounds().OffsetFromOrigin();
  std::vector<double> expected_values = {
      8 + view_offset.x(), 16 + view_offset.y(), 49, 17,
      8 + view_offset.x(), 34 + view_offset.y(), 44, 17};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);
}

}  // namespace content
