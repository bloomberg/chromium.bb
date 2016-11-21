// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/log_web_ui_url.h"

#include <stdint.h>

#include <vector>

#include "base/hash.h"
#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/features/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using base::Bucket;
using testing::ElementsAre;

namespace webui {

class LogWebUIUrlTest : public InProcessBrowserTest {
 public:
  LogWebUIUrlTest() {}
  ~LogWebUIUrlTest() override {}

  std::vector<Bucket> GetSamples() {
    return histogram_tester_.GetAllSamples(webui::kWebUICreatedForUrl);
  }

  void SetUpOnMainThread() override {
    // Disable MD History to test non-MD history page.
    scoped_feature_list_.InitAndDisableFeature(
        features::kMaterialDesignHistory);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(LogWebUIUrlTest);
};

IN_PROC_BROWSER_TEST_F(LogWebUIUrlTest, TestHistoryFrame) {
  GURL history_frame_url(chrome::kChromeUIHistoryFrameURL);

  ui_test_utils::NavigateToURL(browser(), history_frame_url);

  uint32_t history_frame_url_hash = base::Hash(history_frame_url.spec());
  EXPECT_THAT(GetSamples(), ElementsAre(Bucket(history_frame_url_hash, 1)));

  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);

  EXPECT_THAT(GetSamples(), ElementsAre(Bucket(history_frame_url_hash, 2)));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_F(LogWebUIUrlTest, TestUberPage) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::string16 history_title = l10n_util::GetStringUTF16(IDS_HISTORY_TITLE);

  {
    content::TitleWatcher title_watcher(tab, history_title);
    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIHistoryURL));
    ASSERT_EQ(history_title, title_watcher.WaitAndGetTitle());
  }

  std::string scheme(content::kChromeUIScheme);
  GURL uber_url(scheme + "://" + chrome::kChromeUIUberHost);
  uint32_t uber_url_hash = base::Hash(uber_url.spec());

  GURL uber_frame_url(chrome::kChromeUIUberFrameURL);
  uint32_t uber_frame_url_hash = base::Hash(uber_frame_url.spec());

  GURL history_frame_url(chrome::kChromeUIHistoryFrameURL);
  uint32_t history_frame_url_hash = base::Hash(history_frame_url.spec());

  EXPECT_THAT(GetSamples(), ElementsAre(Bucket(history_frame_url_hash, 1),
                                        Bucket(uber_frame_url_hash, 1),
                                        Bucket(uber_url_hash, 1)));

  {
    content::TitleWatcher title_watcher(tab, history_title);
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    ASSERT_EQ(history_title, title_watcher.WaitAndGetTitle());
  }

  EXPECT_THAT(GetSamples(), ElementsAre(Bucket(history_frame_url_hash, 2),
                                        Bucket(uber_frame_url_hash, 2),
                                        Bucket(uber_url_hash, 2)));

  {
    // Pretend a user clicked on "Extensions".
    base::string16 extensions_title =
        l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE);
    content::TitleWatcher title_watcher(tab, extensions_title);
    std::string javascript =
        "uber.invokeMethodOnWindow(window, 'showPage', {pageId: 'extensions'})";
    ASSERT_TRUE(content::ExecuteScript(tab, javascript));
    ASSERT_EQ(extensions_title, title_watcher.WaitAndGetTitle());
  }

  GURL extensions_frame_url(chrome::kChromeUIExtensionsFrameURL);
  uint32_t extensions_frame_url_hash = base::Hash(extensions_frame_url.spec());

  EXPECT_THAT(GetSamples(), ElementsAre(Bucket(extensions_frame_url_hash, 1),
                                        Bucket(history_frame_url_hash, 2),
                                        Bucket(uber_frame_url_hash, 2),
                                        Bucket(uber_url_hash, 2)));
}
#endif

}  // namespace webui
