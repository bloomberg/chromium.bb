// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"

#include "extensions/browser/extension_prefs.h"
#include "extensions/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "url/gurl.h"

using extensions::Extension;
using extensions::MenuItem;
using extensions::MenuManager;
using extensions::MenuManagerFactory;
using extensions::URLPatternSet;

namespace {

// Generates a ContextMenuParams that matches the specified contexts.
static content::ContextMenuParams CreateParams(int contexts) {
  content::ContextMenuParams rv;
  rv.is_editable = false;
  rv.media_type = blink::WebContextMenuData::MediaTypeNone;
  rv.page_url = GURL("http://test.page/");

  static const base::char16 selected_text[] = { 's', 'e', 'l', 0 };
  if (contexts & MenuItem::SELECTION)
    rv.selection_text = selected_text;

  if (contexts & MenuItem::LINK)
    rv.link_url = GURL("http://test.link/");

  if (contexts & MenuItem::EDITABLE)
    rv.is_editable = true;

  if (contexts & MenuItem::IMAGE) {
    rv.src_url = GURL("http://test.image/");
    rv.media_type = blink::WebContextMenuData::MediaTypeImage;
  }

  if (contexts & MenuItem::VIDEO) {
    rv.src_url = GURL("http://test.video/");
    rv.media_type = blink::WebContextMenuData::MediaTypeVideo;
  }

  if (contexts & MenuItem::AUDIO) {
    rv.src_url = GURL("http://test.audio/");
    rv.media_type = blink::WebContextMenuData::MediaTypeAudio;
  }

  if (contexts & MenuItem::FRAME)
    rv.frame_url = GURL("http://test.frame/");

  return rv;
}

}  // namespace

class RenderViewContextMenuTest : public testing::Test {
 protected:
  // Proxy defined here to minimize friend classes in RenderViewContextMenu
  static bool ExtensionContextAndPatternMatch(
      const content::ContextMenuParams& params,
      MenuItem::ContextList contexts,
      const URLPatternSet& patterns) {
    return RenderViewContextMenu::ExtensionContextAndPatternMatch(
        params, contexts, patterns);
  }

  // Returns a test item.
  MenuItem* CreateTestItem(const Extension* extension, int uid) {
    MenuItem::Type type = MenuItem::NORMAL;
    MenuItem::ContextList contexts(MenuItem::ALL);
    const MenuItem::ExtensionKey key(extension->id());
    bool incognito = false;
    MenuItem::Id id(incognito, key);
    id.uid = uid;
    return new MenuItem(id, "Added by an extension", false, true, type,
                        contexts);
  }

  // Returns a test context menu.
  TestRenderViewContextMenu* CreateContextMenu(
      TestingProfile* profile,
      content::WebContents* web_contents) {
    content::ContextMenuParams params = CreateParams(MenuItem::LINK);
    params.unfiltered_link_url = params.link_url;
    TestRenderViewContextMenu* menu =
        new TestRenderViewContextMenu(web_contents->GetMainFrame(), params);
    // TestingProfile (returned by profile()) does not provide a protocol
    // registry.
    scoped_ptr<ProtocolHandlerRegistry> registry_(
        new ProtocolHandlerRegistry(profile, NULL));
    menu->protocol_handler_registry_ = registry_.get();
    menu->Init();
    return menu;
  }
 private:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
};

// Generates a URLPatternSet with a single pattern
static URLPatternSet CreatePatternSet(const std::string& pattern) {
  URLPattern target(URLPattern::SCHEME_HTTP);
  target.Parse(pattern);

  URLPatternSet rv;
  rv.AddPattern(target);

  return rv;
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForPage) {
  content::ContextMenuParams params = CreateParams(0);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForLink) {
  content::ContextMenuParams params = CreateParams(MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::LINK);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForImage) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForVideo) {
  content::ContextMenuParams params = CreateParams(MenuItem::VIDEO);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::VIDEO);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForAudio) {
  content::ContextMenuParams params = CreateParams(MenuItem::AUDIO);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::AUDIO);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, MatchWhenLinkedImageMatchesTarget) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE |
                                                   MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::LINK);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.link/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, MatchWhenLinkedImageMatchesSource) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE |
                                                   MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::LINK);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.image/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, NoMatchWhenLinkedImageMatchesNeither) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE |
                                                   MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::LINK);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForFrame) {
  content::ContextMenuParams params = CreateParams(MenuItem::FRAME);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::FRAME);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForEditable) {
  content::ContextMenuParams params = CreateParams(MenuItem::EDITABLE);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::EDITABLE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForSelection) {
  content::ContextMenuParams params =
      CreateParams(MenuItem::SELECTION);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::SELECTION);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForSelectionOnLink) {
  content::ContextMenuParams params = CreateParams(
      MenuItem::SELECTION | MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::SELECTION);
  contexts.Add(MenuItem::LINK);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForSelectionOnImage) {
  content::ContextMenuParams params = CreateParams(
      MenuItem::SELECTION | MenuItem::IMAGE);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::SELECTION);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, ItemWithSameTitleFromTwoExtensions) {
  extensions::TestExtensionEnvironment env;

  MenuManager* menu_manager =  // Owned by env.profile().
      static_cast<MenuManager*>(
          (MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
              env.profile(),
              &MenuManagerFactory::BuildServiceInstanceForTesting)));

  const Extension* extension1 =
      env.MakeExtension(base::DictionaryValue(),
                        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const Extension* extension2 =
      env.MakeExtension(base::DictionaryValue(),
                        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

  // Create two items in two extensions with same title.
  MenuItem* item1 = CreateTestItem(extension1, 1);
  ASSERT_TRUE(menu_manager->AddContextItem(extension1, item1));
  MenuItem* item2 = CreateTestItem(extension2, 2);
  ASSERT_TRUE(menu_manager->AddContextItem(extension2, item2));

  scoped_ptr<content::WebContents> web_contents = env.MakeTab();
  scoped_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(env.profile(), web_contents.get()));

  const ui::MenuModel& model = menu->menu_model();
  base::string16 expected_title = base::ASCIIToUTF16("Added by an extension");
  int num_items_found = 0;
  for (int i = 0; i < model.GetItemCount(); ++i) {
    if (expected_title == model.GetLabelAt(i))
      ++num_items_found;
  }

  // Expect both items to be found.
  ASSERT_EQ(2, num_items_found);
}

class RenderViewContextMenuPrefsTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    registry_.reset(new ProtocolHandlerRegistry(profile(), NULL));
  }

  void TearDown() override {
    registry_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestRenderViewContextMenu* CreateContextMenu() {
    content::ContextMenuParams params = CreateParams(MenuItem::LINK);
    params.unfiltered_link_url = params.link_url;
    content::WebContents* wc = web_contents();
    TestRenderViewContextMenu* menu = new TestRenderViewContextMenu(
        wc->GetMainFrame(), params);
    // TestingProfile (returned by profile()) does not provide a protocol
    // registry.
    menu->protocol_handler_registry_ = registry_.get();
    menu->Init();
    return menu;
  }

  void AppendImageItems(TestRenderViewContextMenu* menu) {
    menu->AppendImageItems();
  }

  void SetupDataReductionProxy(bool enable_data_reduction_proxy) {
    drp_test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .WithParamsFlags(
                 data_reduction_proxy::DataReductionProxyParams::kAllowed |
                 data_reduction_proxy::DataReductionProxyParams::
                     kFallbackAllowed |
                 data_reduction_proxy::DataReductionProxyParams::kPromoAllowed)
            .WithMockConfig()
            .SkipSettingsInitialization()
            .Build();

    DataReductionProxyChromeSettings* settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            profile());

    // TODO(bengr): Remove proxy_config::prefs::kProxy registration after M46.
    // See http://crbug.com/445599.
    PrefRegistrySimple* registry =
        drp_test_context_->pref_service()->registry();
    registry->RegisterDictionaryPref(proxy_config::prefs::kProxy);

    drp_test_context_->pref_service()->SetBoolean(
        data_reduction_proxy::prefs::kDataReductionProxyEnabled,
        enable_data_reduction_proxy);
    drp_test_context_->InitSettings();

    settings->InitDataReductionProxySettings(
        drp_test_context_->io_data(), drp_test_context_->pref_service(),
        drp_test_context_->request_context_getter(),
        make_scoped_ptr(new data_reduction_proxy::DataStore()),
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());
  }

  // Force destruction of |DataReductionProxySettings| so that objects on DB
  // task runner can be destroyed before test threads are destroyed. This method
  // must be called by tests that call |SetupDataReductionProxy|. We cannot
  // destroy |drp_test_context_| until browser context keyed services are
  // destroyed since |DataReductionProxyChromeSettings| holds a pointer to the
  // |PrefService|, which is owned by |drp_test_context_|.
  void DestroyDataReductionProxySettings() {
    drp_test_context_->DestroySettings();
  }

 protected:
  scoped_ptr<data_reduction_proxy::DataReductionProxyTestContext>
      drp_test_context_;

 private:
  scoped_ptr<ProtocolHandlerRegistry> registry_;
};

// Verifies when Incognito Mode is not available (disabled by policy),
// Open Link in Incognito Window link in the context menu is disabled.
TEST_F(RenderViewContextMenuPrefsTest,
       DisableOpenInIncognitoWindowWhenIncognitoIsDisabled) {
  scoped_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());

  // Initially the Incognito mode is be enabled. So is the Open Link in
  // Incognito Window link.
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
  EXPECT_TRUE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));

  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  menu.reset(CreateContextMenu());
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
  EXPECT_FALSE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
}

// Make sure the checking custom command id that is not enabled will not
// cause DCHECK failure.
TEST_F(RenderViewContextMenuPrefsTest,
       IsCustomCommandIdEnabled) {
  scoped_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());

  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_CUSTOM_FIRST));
}

// Verify that request headers specify that data reduction proxy should return
// the original non compressed resource when "Save Image As..." is used with
// Data Saver enabled.
TEST_F(RenderViewContextMenuPrefsTest, DataSaverEnabledSaveImageAs) {
  SetupDataReductionProxy(true);

  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.unfiltered_link_url = params.link_url;
  content::WebContents* wc = web_contents();
  scoped_ptr<TestRenderViewContextMenu> menu(
      new TestRenderViewContextMenu(wc->GetMainFrame(), params));

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 0);

  const std::string& headers =
      content::WebContentsTester::For(web_contents())->GetSaveFrameHeaders();
  EXPECT_TRUE(headers.find("Chrome-Proxy: pass-through") != std::string::npos);
  EXPECT_TRUE(headers.find("Cache-Control: no-cache") != std::string::npos);

  DestroyDataReductionProxySettings();
}

// Verify that request headers do not specify pass through when "Save Image
// As..." is used with Data Saver disabled.
TEST_F(RenderViewContextMenuPrefsTest, DataSaverDisabledSaveImageAs) {
  SetupDataReductionProxy(false);

  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.unfiltered_link_url = params.link_url;
  content::WebContents* wc = web_contents();
  scoped_ptr<TestRenderViewContextMenu> menu(
      new TestRenderViewContextMenu(wc->GetMainFrame(), params));

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 0);

  const std::string& headers =
      content::WebContentsTester::For(web_contents())->GetSaveFrameHeaders();
  EXPECT_TRUE(headers.find("Chrome-Proxy: pass-through") == std::string::npos);
  EXPECT_TRUE(headers.find("Cache-Control: no-cache") == std::string::npos);

  DestroyDataReductionProxySettings();
}

// Verify that the Chrome-Proxy Lo-Fi directive causes the context menu to
// display the "Load Image" menu item.
TEST_F(RenderViewContextMenuPrefsTest, DataSaverLoadImage) {
  SetupDataReductionProxy(true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.properties[data_reduction_proxy::chrome_proxy_header()] =
      data_reduction_proxy::chrome_proxy_lo_fi_directive();
  params.unfiltered_link_url = params.link_url;
  content::WebContents* wc = web_contents();
  scoped_ptr<TestRenderViewContextMenu> menu(
      new TestRenderViewContextMenu(wc->GetMainFrame(), params));
  AppendImageItems(menu.get());

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE));

  DestroyDataReductionProxySettings();
}
