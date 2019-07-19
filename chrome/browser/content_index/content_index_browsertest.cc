// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/common/content_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

using offline_items_collection::ContentId;
using offline_items_collection::OfflineContentProvider;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemVisuals;

namespace {

std::string GetDescriptionIdFromOfflineItemKey(const std::string& id) {
  std::string description_id;
  bool result =
      re2::RE2::FullMatch(id, re2::RE2("\\d+#[^#]+#(.*)"), &description_id);
  EXPECT_TRUE(result);
  return description_id;
}

class ContentIndexTest : public InProcessBrowserTest,
                         public OfflineContentProvider::Observer {
 public:
  ContentIndexTest() = default;
  ~ContentIndexTest() override = default;

  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server_->Start());

    ui_test_utils::NavigateToURL(
        browser(), https_server_->GetURL("/content_index/content_index.html"));

    RunScript("RegisterServiceWorker()");

    auto* provider = browser()->profile()->GetContentIndexProvider();
    DCHECK(provider);
    provider_ = static_cast<ContentIndexProviderImpl*>(provider);
    provider_->AddObserver(this);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  // Runs |script| and expects it to complete successfully. |script| must
  // result in a Promise. Returns the resolved contents of the Promise.
  std::string RunScript(const std::string& script) {
    std::string result;
    RunScript(script, &result);
    return result.substr(5);  // Ignore the trailing `ok - `.
  }

  // OfflineContentProvider::Observer implementation:
  void OnItemsAdded(
      const OfflineContentProvider::OfflineItemList& items) override {
    ASSERT_EQ(items.size(), 1u);
    offline_items_[GetDescriptionIdFromOfflineItemKey(items[0].id.id)] =
        items[0];
  }

  void OnItemRemoved(const ContentId& id) override {
    offline_items_.erase(GetDescriptionIdFromOfflineItemKey(id.id));
  }

  void OnItemUpdated(
      const OfflineItem& item,
      const base::Optional<offline_items_collection::UpdateDelta>& update_delta)
      override {
    std::string id = GetDescriptionIdFromOfflineItemKey(item.id.id);
    ASSERT_TRUE(offline_items_.count(id));
    offline_items_[id] = item;
  }

  std::map<std::string, OfflineItem>& offline_items() { return offline_items_; }
  ContentIndexProviderImpl* provider() { return provider_; }

 private:
  void RunScript(const std::string& script, std::string* result) {
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
        "WrapFunction(async () => " + script + ")", result));
    ASSERT_TRUE(
        base::StartsWith(*result, "ok - ", base::CompareCase::SENSITIVE))
        << "Unexpected result: " << *result;
  }

  std::map<std::string, OfflineItem> offline_items_;
  ContentIndexProviderImpl* provider_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(ContentIndexTest, OfflineItemObserversReceiveEvents) {
  RunScript("AddContent('my-id-1')");
  RunScript("AddContent('my-id-2')");
  base::RunLoop().RunUntilIdle();  // Wait for the provider to get the content.

  EXPECT_EQ(offline_items().size(), 2u);
  ASSERT_TRUE(offline_items().count("my-id-1"));
  EXPECT_TRUE(offline_items().count("my-id-2"));
  EXPECT_EQ(RunScript("GetIds()"), "my-id-1,my-id-2");

  std::string description1 = offline_items().at("my-id-1").description;
  RunScript("AddContent('my-id-1')");     // update
  RunScript("DeleteContent('my-id-2')");  // delete
  base::RunLoop().RunUntilIdle();  // Wait for the provider to get the content.

  EXPECT_EQ(offline_items().size(), 1u);
  ASSERT_TRUE(offline_items().count("my-id-1"));
  EXPECT_FALSE(offline_items().count("my-id-2"));

  // Expect the description to have been updated.
  EXPECT_NE(description1, offline_items().at("my-id-1").description);
}

IN_PROC_BROWSER_TEST_F(ContentIndexTest, GetVisuals) {
  RunScript("AddContent('my-id')");
  base::RunLoop().RunUntilIdle();  // Wait for the provider to get the content.

  base::RunLoop run_loop;
  SkBitmap icon;
  provider()->GetVisualsForItem(
      offline_items().at("my-id").id,
      OfflineContentProvider::GetVisualsOptions(),
      base::BindLambdaForTesting(
          [&](const ContentId& id,
              std::unique_ptr<OfflineItemVisuals> visuals) {
            ASSERT_EQ(offline_items().at("my-id").id, id);
            ASSERT_TRUE(visuals);
            icon = visuals->icon.AsBitmap();
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_FALSE(icon.isNull());
  EXPECT_FALSE(icon.drawsNothing());
}

IN_PROC_BROWSER_TEST_F(ContentIndexTest, LaunchUrl) {
  RunScript("AddContent('my-id')");
  base::RunLoop().RunUntilIdle();  // Wait for the provider to get the content.

  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  GURL current_url =
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_TRUE(base::EndsWith(current_url.spec(),
                             "/content_index/content_index.html",
                             base::CompareCase::SENSITIVE));

  provider()->OpenItem(offline_items_collection::LaunchLocation::DOWNLOAD_HOME,
                       offline_items().at("my-id").id);
  base::RunLoop().RunUntilIdle();  // Wait for the page to open.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  current_url = browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_TRUE(base::EndsWith(current_url.spec(),
                             "/content_index/content_index.html?launch",
                             base::CompareCase::SENSITIVE));
}

}  // namespace
