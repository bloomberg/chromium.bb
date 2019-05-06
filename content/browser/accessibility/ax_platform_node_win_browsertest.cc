// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win.h"

#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_com_win.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

using Microsoft::WRL::ComPtr;

namespace content {
class AXPlatformNodeWinBrowserTest : public ContentBrowserTest {
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

  template <typename T>
  ComPtr<T> QueryInterfaceFromNode(
      BrowserAccessibility* browser_accessibility) {
    ComPtr<T> result;
    EXPECT_HRESULT_SUCCEEDED(
        browser_accessibility->GetNativeViewAccessible()->QueryInterface(
            __uuidof(T), &result));
    return result;
  }

  ComPtr<IAccessible> IAccessibleFromNode(
      BrowserAccessibility* browser_accessibility) {
    return QueryInterfaceFromNode<IAccessible>(browser_accessibility);
  }

  ComPtr<IAccessible2> ToIAccessible2(ComPtr<IAccessible> accessible) {
    CHECK(accessible);
    ComPtr<IServiceProvider> service_provider;
    accessible.As(&service_provider);
    ComPtr<IAccessible2> result;
    CHECK(SUCCEEDED(service_provider->QueryService(IID_IAccessible2,
                                                   IID_PPV_ARGS(&result))));
    return result;
  }

  void UIAGetPropertyValueFlowsFromBrowserTestTemplate(
      const BrowserAccessibility* target_browser_accessibility,
      const std::vector<std::string>& expected_names) {
    ASSERT_NE(nullptr, target_browser_accessibility);

    auto* target_browser_accessibility_com_win =
        ToBrowserAccessibilityWin(target_browser_accessibility)->GetCOM();
    ASSERT_NE(nullptr, target_browser_accessibility_com_win);

    base::win::ScopedVariant flows_from_variant;
    target_browser_accessibility_com_win->GetPropertyValue(
        UIA_FlowsFromPropertyId, flows_from_variant.Receive());
    ASSERT_EQ(VT_ARRAY | VT_UNKNOWN, flows_from_variant.type());
    ASSERT_EQ(1u, SafeArrayGetDim(V_ARRAY(flows_from_variant.ptr())));

    LONG lower_bound, upper_bound, size;
    ASSERT_HRESULT_SUCCEEDED(
        SafeArrayGetLBound(V_ARRAY(flows_from_variant.ptr()), 1, &lower_bound));
    ASSERT_HRESULT_SUCCEEDED(
        SafeArrayGetUBound(V_ARRAY(flows_from_variant.ptr()), 1, &upper_bound));
    size = upper_bound - lower_bound + 1;
    ASSERT_EQ(static_cast<LONG>(expected_names.size()), size);

    std::vector<std::string> names;
    for (LONG i = 0; i < size; ++i) {
      CComPtr<IUnknown> unknown_element = nullptr;
      ASSERT_HRESULT_SUCCEEDED(SafeArrayGetElement(
          V_ARRAY(flows_from_variant.ptr()), &i, &unknown_element));
      ASSERT_NE(nullptr, unknown_element);

      CComPtr<IRawElementProviderSimple> raw_element_provider_simple = nullptr;
      ASSERT_HRESULT_SUCCEEDED(
          unknown_element->QueryInterface(&raw_element_provider_simple));
      ASSERT_NE(nullptr, raw_element_provider_simple);

      base::win::ScopedVariant name;
      ASSERT_HRESULT_SUCCEEDED(raw_element_provider_simple->GetPropertyValue(
          UIA_NamePropertyId, name.Receive()));
      ASSERT_EQ(VT_BSTR, name.type());
      names.push_back(base::UTF16ToUTF8(
          std::wstring(V_BSTR(name.ptr()), SysStringLen(V_BSTR(name.ptr())))));
    }

    ASSERT_THAT(names, testing::UnorderedElementsAreArray(expected_names));
  }

  void UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role expected_role,
      content::BrowserAccessibility* (content::BrowserAccessibility::*f)(
          uint32_t) const,
      uint32_t index_arg,
      bool expected_is_modal,
      bool expected_is_window_provider_available) {
    BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();
    BrowserAccessibilityComWin* root_browser_accessibility_com_win =
        ToBrowserAccessibilityWin(root_browser_accessibility)->GetCOM();
    ASSERT_NE(nullptr, root_browser_accessibility_com_win);

    BrowserAccessibility* browser_accessibility =
        (root_browser_accessibility->*f)(index_arg);
    ASSERT_NE(nullptr, browser_accessibility);
    ASSERT_EQ(expected_role, browser_accessibility->GetRole());
    BrowserAccessibilityComWin* browser_accessibility_com_win =
        ToBrowserAccessibilityWin(browser_accessibility)->GetCOM();
    ASSERT_NE(nullptr, browser_accessibility_com_win);

    ComPtr<IWindowProvider> window_provider = nullptr;
    ASSERT_HRESULT_SUCCEEDED(browser_accessibility_com_win->GetPatternProvider(
        UIA_WindowPatternId, &window_provider));
    if (expected_is_window_provider_available) {
      ASSERT_NE(nullptr, window_provider.Get());

      BOOL is_modal = FALSE;
      ASSERT_HRESULT_SUCCEEDED(window_provider->get_IsModal(&is_modal));
      ASSERT_EQ(expected_is_modal, is_modal);
    } else {
      ASSERT_EQ(nullptr, window_provider.Get());
    }
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

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       IA2ScrollToPointIframeText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/iframe-text.html");
  WaitForAccessibilityTreeToContainNodeWithName(
      shell()->web_contents(),
      "Game theory is \"the study of Mathematical model mathematical models of "
      "conflict and cooperation between intelligent rational decision-makers."
      "\"");

  BrowserAccessibility* browser_accessibility =
      GetRootAndAssertNonNull()->PlatformDeepestLastChild();
  ASSERT_NE(nullptr, browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kStaticText, browser_accessibility->GetRole());

  BrowserAccessibility* iframe_browser_accessibility =
      browser_accessibility->manager()->GetRoot();
  ASSERT_NE(nullptr, iframe_browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea,
            iframe_browser_accessibility->GetRole());

  gfx::Rect iframe_screen_bounds = iframe_browser_accessibility->GetBoundsRect(
      ui::AXCoordinateSystem::kScreen, ui::AXClippingBehavior::kUnclipped);

  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);
  ComPtr<IAccessible2> root_iaccessible2 =
      ToIAccessible2(IAccessibleFromNode(browser_accessibility));
  ASSERT_EQ(S_OK, root_iaccessible2->scrollToPoint(
                      IA2_COORDTYPE_SCREEN_RELATIVE, iframe_screen_bounds.x(),
                      iframe_screen_bounds.y()));
  location_changed_waiter.WaitForNotification();

  gfx::Rect bounds = browser_accessibility->GetBoundsRect(
      ui::AXCoordinateSystem::kScreen, ui::AXClippingBehavior::kUnclipped);
  ASSERT_EQ(iframe_screen_bounds.y(), bounds.y());
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAGetPropertyValueFlowsFromNone) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/aria/aria-label.html");

  UIAGetPropertyValueFlowsFromBrowserTestTemplate(
      FindNode(ax::mojom::Role::kCheckBox, "aria label"), {});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAGetPropertyValueFlowsFromSingle) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/aria/aria-flowto.html");

  UIAGetPropertyValueFlowsFromBrowserTestTemplate(
      FindNode(ax::mojom::Role::kFooter, "next"), {"current"});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAGetPropertyValueFlowsFromMultiple) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/aria/aria-flowto-multiple.html");

  UIAGetPropertyValueFlowsFromBrowserTestTemplate(
      FindNode(ax::mojom::Role::kGenericContainer, "b3"), {"a3", "c3"});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDialog) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <dialog open>Example Text</dialog>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDialogAriaModalFalse) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <dialog open aria-modal="false">Example Text</dialog>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDialogAriaModalTrue) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <dialog open aria-modal="true">Example Text</dialog>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      true, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDiv) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div>Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kGenericContainer,
      &BrowserAccessibility::PlatformGetChild, 0, false, false);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDivAriaModalFalse) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div aria-modal="false">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kGenericContainer,
      &BrowserAccessibility::PlatformGetChild, 0, false, false);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDivAriaModalTrue) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div aria-modal="true">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kGenericContainer,
      &BrowserAccessibility::PlatformGetChild, 0, false, false);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDivDialog) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="dialog">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDivDialogAriaModalFalse) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="dialog" aria-modal="false">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDivDialogAriaModalTrue) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="dialog" aria-modal="true">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      true, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDivAlertDialog) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="alertdialog">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kAlertDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(
    AXPlatformNodeWinBrowserTest,
    UIAIWindowProviderGetIsModalOnDivAlertDialogAriaModalFalse) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="alertdialog" aria-modal="false">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kAlertDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(
    AXPlatformNodeWinBrowserTest,
    UIAIWindowProviderGetIsModalOnDivAlertDialogAriaModalTrue) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="alertdialog" aria-modal="true">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kAlertDialog, &BrowserAccessibility::PlatformGetChild, 0,
      true, true);
}
}  // namespace content
