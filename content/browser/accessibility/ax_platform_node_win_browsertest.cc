// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win.h"

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_com_win.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "net/base/escape.h"

using Microsoft::WRL::ComPtr;

namespace content {
class AXPlatformNodeWinBrowserTest : public ContentBrowserTest {
 protected:
  void LoadInitialAccessibilityTreeFromHtml(
      const std::string& html,
      ui::AXMode accessibility_mode = ui::kAXModeComplete) {
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           accessibility_mode,
                                           ax::mojom::Event::kLoadComplete);
    GURL html_data_url =
        GURL("data:text/html," + net::EscapeQueryParamValue(html, false));
    NavigateToURL(shell(), html_data_url);
    waiter.WaitForNotification();
  }

  void UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role expected_role,
      content::BrowserAccessibility* (content::BrowserAccessibility::*f)(
          uint32_t) const,
      uint32_t index_arg,
      bool expected_is_modal,
      bool expected_is_window_provider_available) {
    WebContentsImpl* web_contents_impl =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    ASSERT_NE(nullptr, web_contents_impl);

    BrowserAccessibilityManager* browser_accessibility_manager =
        web_contents_impl->GetRootBrowserAccessibilityManager();
    ASSERT_NE(nullptr, browser_accessibility_manager);

    BrowserAccessibility* root_browser_accessibility =
        browser_accessibility_manager->GetRoot();
    ASSERT_NE(nullptr, root_browser_accessibility);
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
};

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
