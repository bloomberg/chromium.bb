// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_infobar_delegate_desktop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/plugins/reload_plugin_infobar_delegate.h"
#include "chrome/browser/previews/previews_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/startup/automation_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/startup/default_browser_infobar_delegate.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/ui/startup/session_crashed_infobar_delegate.h"
#endif

#if !defined(USE_AURA)
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#endif

class InfoBarsTest : public InProcessBrowserTest {
 public:
  InfoBarsTest() {}

  void InstallExtension(const char* filename) {
    base::FilePath path = ui_test_utils::GetTestFilePath(
        base::FilePath().AppendASCII("extensions"),
        base::FilePath().AppendASCII(filename));
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();

    extensions::TestExtensionRegistryObserver observer(
        extensions::ExtensionRegistry::Get(browser()->profile()));

    std::unique_ptr<ExtensionInstallPrompt> client(new ExtensionInstallPrompt(
        browser()->tab_strip_model()->GetActiveWebContents()));
    scoped_refptr<extensions::CrxInstaller> installer(
        extensions::CrxInstaller::Create(service, std::move(client)));
    installer->set_install_cause(extension_misc::INSTALL_CAUSE_AUTOMATION);
    installer->InstallCrx(path);

    observer.WaitForExtensionLoaded();
  }
};

IN_PROC_BROWSER_TEST_F(InfoBarsTest, TestInfoBarsCloseOnNewTheme) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html"));

  content::WindowedNotificationObserver infobar_added_1(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
        content::NotificationService::AllSources());
  InstallExtension("theme.crx");
  infobar_added_1.Wait();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/simple.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WindowedNotificationObserver infobar_added_2(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
        content::NotificationService::AllSources());
  content::WindowedNotificationObserver infobar_removed_1(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
        content::NotificationService::AllSources());
  InstallExtension("theme2.crx");
  infobar_removed_1.Wait();
  infobar_added_2.Wait();
  EXPECT_EQ(
      0u,
      InfoBarService::FromWebContents(
          browser()->tab_strip_model()->GetWebContentsAt(0))->infobar_count());

  content::WindowedNotificationObserver infobar_removed_2(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
        content::NotificationService::AllSources());
  ThemeServiceFactory::GetForProfile(browser()->profile())->UseDefaultTheme();
  infobar_removed_2.Wait();
  EXPECT_EQ(0u,
            InfoBarService::FromWebContents(
                browser()->tab_strip_model()->GetActiveWebContents())->
                infobar_count());
}

namespace {

// Helper to return when an InfoBar has been removed or replaced.
class InfoBarObserver : public infobars::InfoBarManager::Observer {
 public:
  InfoBarObserver(infobars::InfoBarManager* manager, infobars::InfoBar* infobar)
      : manager_(manager), infobar_(infobar) {
    manager_->AddObserver(this);
  }

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
    if (infobar != infobar_)
      return;
    manager_->RemoveObserver(this);
    run_loop_.Quit();
  }
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override {
    OnInfoBarRemoved(old_infobar, false);
  }

  void WaitForRemoval() { run_loop_.Run(); }

 private:
  infobars::InfoBarManager* manager_;
  infobars::InfoBar* infobar_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarObserver);
};

}  // namespace

class InfoBarUiTest : public UiBrowserTest {
 public:
  InfoBarUiTest() = default;

  // TestBrowserUi:
  void PreShow() override;
  void ShowUi(const std::string& name) override;
  bool VerifyUi() override;
  void WaitForUserDismissal() override;

 private:
  // Returns the active tab.
  content::WebContents* GetWebContents();

  // Returns the InfoBarService associated with the active tab.
  InfoBarService* GetInfoBarService();

  // Sets |infobars_| to a sorted (by pointer value) list of all infobars from
  // the active tab.
  void UpdateInfoBars();

  infobars::InfoBarManager::InfoBars infobars_;
  infobars::InfoBarDelegate::InfoBarIdentifier desired_infobar_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarUiTest);
};

void InfoBarUiTest::PreShow() {
  UpdateInfoBars();
}

void InfoBarUiTest::ShowUi(const std::string& name) {
  using IBD = infobars::InfoBarDelegate;
  const base::flat_map<std::string, IBD::InfoBarIdentifier> kIdentifiers = {
      {"app_banner", IBD::APP_BANNER_INFOBAR_DELEGATE},
      {"reload_plugin", IBD::RELOAD_PLUGIN_INFOBAR_DELEGATE},
      {"file_access_disabled", IBD::FILE_ACCESS_DISABLED_INFOBAR_DELEGATE},
      {"collected_cookies", IBD::COLLECTED_COOKIES_INFOBAR_DELEGATE},
      {"alternate_nav", IBD::ALTERNATE_NAV_INFOBAR_DELEGATE},
      {"default_browser", IBD::DEFAULT_BROWSER_INFOBAR_DELEGATE},
      {"session_crashed", IBD::SESSION_CRASHED_INFOBAR_DELEGATE_MAC_IOS},
      {"page_info", IBD::PAGE_INFO_INFOBAR_DELEGATE},
      {"translate", IBD::TRANSLATE_INFOBAR_DELEGATE_NON_AURA},
      {"data_reduction_proxy_preview",
       IBD::DATA_REDUCTION_PROXY_PREVIEW_INFOBAR_DELEGATE},
      {"automation", IBD::AUTOMATION_INFOBAR_DELEGATE},
  };
  auto id = kIdentifiers.find(name);
  desired_infobar_ = (id == kIdentifiers.end()) ? IBD::INVALID : id->second;

  switch (desired_infobar_) {
    case IBD::APP_BANNER_INFOBAR_DELEGATE:
      banners::AppBannerInfoBarDelegateDesktop::Create(
          GetWebContents(), nullptr, nullptr, content::Manifest());
      break;
    case IBD::RELOAD_PLUGIN_INFOBAR_DELEGATE:
      ReloadPluginInfoBarDelegate::Create(
          GetInfoBarService(), nullptr,
          l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT,
                                     base::ASCIIToUTF16("Test Plugin")));
      break;
    case IBD::FILE_ACCESS_DISABLED_INFOBAR_DELEGATE:
      ChromeSelectFilePolicy(GetWebContents()).SelectFileDenied();
      break;
    case IBD::COLLECTED_COOKIES_INFOBAR_DELEGATE:
      CollectedCookiesInfoBarDelegate::Create(GetInfoBarService());
      break;
    case IBD::ALTERNATE_NAV_INFOBAR_DELEGATE: {
      AutocompleteMatch match;
      match.destination_url = GURL("http://intranetsite/");
      AlternateNavInfoBarDelegate::Create(GetWebContents(), base::string16(),
                                          match, GURL("http://example.com/"));
      break;
    }
    case IBD::DEFAULT_BROWSER_INFOBAR_DELEGATE:
#if defined(OS_CHROMEOS)
      ADD_FAILURE() << "This infobar is not supported on this OS.";
#else
      chrome::DefaultBrowserInfoBarDelegate::Create(GetInfoBarService(),
                                                    browser()->profile());
#endif
      break;
    case IBD::SESSION_CRASHED_INFOBAR_DELEGATE_MAC_IOS:
#if defined(OS_MACOSX)
      SessionCrashedInfoBarDelegate::Create(browser());
#else
      ADD_FAILURE() << "This infobar is not supported on this OS.";
#endif
      break;
    case IBD::PAGE_INFO_INFOBAR_DELEGATE:
      PageInfoInfoBarDelegate::Create(GetInfoBarService());
      break;
    case IBD::TRANSLATE_INFOBAR_DELEGATE_NON_AURA: {
#if defined(USE_AURA)
      ADD_FAILURE() << "This infobar is not supported on this toolkit.";
#else
      ChromeTranslateClient::CreateForWebContents(GetWebContents());
      ChromeTranslateClient* translate_client =
          ChromeTranslateClient::FromWebContents(GetWebContents());
      translate::TranslateInfoBarDelegate::Create(
          false, translate_client->GetTranslateManager()->GetWeakPtr(),
          GetInfoBarService(), false,
          translate::TRANSLATE_STEP_BEFORE_TRANSLATE, "ja", "en",
          translate::TranslateErrors::NONE, false);
#endif
      break;
    }
    case IBD::DATA_REDUCTION_PROXY_PREVIEW_INFOBAR_DELEGATE:
      PreviewsInfoBarDelegate::Create(
          GetWebContents(), previews::PreviewsType::LOFI, base::Time(), true,
          false, PreviewsInfoBarDelegate::OnDismissPreviewsInfobarCallback(),
          nullptr);
      break;
    case IBD::AUTOMATION_INFOBAR_DELEGATE:
      AutomationInfoBarDelegate::Create();
      break;
    default:
      break;
  }
}

bool InfoBarUiTest::VerifyUi() {
  infobars::InfoBarManager::InfoBars old_infobars = infobars_;
  UpdateInfoBars();
  auto added = base::STLSetDifference<infobars::InfoBarManager::InfoBars>(
      infobars_, old_infobars);
  return (added.size() == 1) &&
         (added[0]->delegate()->GetIdentifier() == desired_infobar_);
}

void InfoBarUiTest::WaitForUserDismissal() {
  InfoBarObserver observer(GetInfoBarService(), infobars_.front());
  observer.WaitForRemoval();
}

content::WebContents* InfoBarUiTest::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

InfoBarService* InfoBarUiTest::GetInfoBarService() {
  return InfoBarService::FromWebContents(GetWebContents());
}

void InfoBarUiTest::UpdateInfoBars() {
  infobars_ = GetInfoBarService()->infobars_;
  std::sort(infobars_.begin(), infobars_.end());
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_app_banner) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_reload_plugin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_file_access_disabled) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_collected_cookies) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_alternate_nav) {
  ShowAndVerifyUi();
}

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_default_browser) {
  ShowAndVerifyUi();
}
#endif

#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_session_crashed) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_page_info) {
  ShowAndVerifyUi();
}

#if !defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_translate) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_data_reduction_proxy_preview) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_automation) {
  ShowAndVerifyUi();
}
