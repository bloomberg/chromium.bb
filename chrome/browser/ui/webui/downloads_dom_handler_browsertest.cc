// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/auto_reset.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/downloads_dom_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// Reads |right_json| into a ListValue |left_list|; returns true if all
// key-value pairs in in all dictionaries in |right_list| are also in the
// corresponding dictionary in |left_list|. Ignores keys in dictionaries in
// |left_list| that are not in the corresponding dictionary in |right_list|.
bool ListMatches(base::ListValue* left_list, const std::string& right_json) {
  scoped_ptr<base::Value> right_value = base::JSONReader::Read(right_json);
  base::ListValue* right_list = NULL;
  CHECK(right_value->GetAsList(&right_list));
  for (size_t i = 0; i < left_list->GetSize(); ++i) {
    base::DictionaryValue* left_dict = NULL;
    base::DictionaryValue* right_dict = NULL;
    CHECK(left_list->GetDictionary(i, &left_dict));
    CHECK(right_list->GetDictionary(i, &right_dict));
    for (base::DictionaryValue::Iterator iter(*right_dict);
          !iter.IsAtEnd(); iter.Advance()) {
      base::Value* left_value = NULL;
      if (left_dict->HasKey(iter.key()) &&
          left_dict->Get(iter.key(), &left_value) &&
          !iter.value().Equals(left_value)) {
        LOG(WARNING) << "key \"" << iter.key() << "\" doesn't match ("
                     << iter.value() << " vs. " << *left_value << ")";
        return false;
      }
    }
  }
  return true;
}

// A |DownloadsDOMHandler| that doesn't use a real WebUI object, but is real in
// all other respects.
class MockDownloadsDOMHandler : public DownloadsDOMHandler {
 public:
  explicit MockDownloadsDOMHandler(content::DownloadManager* download_manager)
    : DownloadsDOMHandler(download_manager),
      waiting_list_(false),
      waiting_updated_(false) {
  }
  ~MockDownloadsDOMHandler() override {}

  base::ListValue* downloads_list() { return downloads_list_.get(); }
  base::DictionaryValue* download_updated() { return download_updated_.get(); }

  void WaitForDownloadsList() {
    if (downloads_list_)
      return;
    base::AutoReset<bool> reset_waiting(&waiting_list_, true);
    content::RunMessageLoop();
  }

  void WaitForDownloadUpdated() {
    if (download_updated_)
      return;
    base::AutoReset<bool> reset_waiting(&waiting_updated_, true);
    content::RunMessageLoop();
  }

  void ForceSendCurrentDownloads() {
    ScheduleSendCurrentDownloads();
  }

  void reset_downloads_list() { downloads_list_.reset(); }
  void reset_download_updated() { download_updated_.reset(); }

  using DownloadsDOMHandler::FinalizeRemovals;

 protected:
  content::WebContents* GetWebUIWebContents() override { return NULL; }

  void CallUpdateAll(const base::ListValue& list) override {
    downloads_list_.reset(list.DeepCopy());
    if (waiting_list_) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::MessageLoop::QuitWhenIdleClosure());
    }
  }

  void CallUpdateItem(const base::DictionaryValue& item) override {
    download_updated_.reset(item.DeepCopy());
    if (waiting_updated_) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::MessageLoop::QuitWhenIdleClosure());
    }
  }

 private:
  scoped_ptr<base::ListValue> downloads_list_;
  scoped_ptr<base::DictionaryValue> download_updated_;
  bool waiting_list_;
  bool waiting_updated_;

  DISALLOW_COPY_AND_ASSIGN(MockDownloadsDOMHandler);
};

}  // namespace

class DownloadsDOMHandlerTest : public InProcessBrowserTest {
 public:
  DownloadsDOMHandlerTest() {}

  ~DownloadsDOMHandlerTest() override {}

  void SetUpOnMainThread() override {
    mock_handler_.reset(new MockDownloadsDOMHandler(download_manager()));
    CHECK(downloads_directory_.CreateUniqueTempDir());
    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory,
        downloads_directory_.path());
    CHECK(embedded_test_server()->Start());
    mock_handler_->HandleGetDownloads(nullptr);
  }

  content::DownloadManager* download_manager() {
    return content::BrowserContext::GetDownloadManager(browser()->profile());
  }

  void DownloadAnItem() {
    GURL url = embedded_test_server()->GetURL("/downloads/image.jpg");
    std::vector<GURL> url_chain;
    url_chain.push_back(url);
    base::Time current(base::Time::Now());
    download_manager()->CreateDownloadItem(
        1, // id
        base::FilePath(FILE_PATH_LITERAL("/path/to/file")),
        base::FilePath(FILE_PATH_LITERAL("/path/to/file")),
        url_chain,
        GURL(std::string()),
        "application/octet-stream",
        "application/octet-stream",
        current,
        current,
        std::string(),
        std::string(),
        128,
        128,
        content::DownloadItem::COMPLETE,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        content::DOWNLOAD_INTERRUPT_REASON_NONE,
        false);

    mock_handler_->ForceSendCurrentDownloads();
    mock_handler_->WaitForDownloadsList();
    ASSERT_EQ(1, static_cast<int>(mock_handler_->downloads_list()->GetSize()));
    EXPECT_TRUE(ListMatches(
        mock_handler_->downloads_list(),
        "[{\"file_externally_removed\": false,"
        "  \"file_name\": \"file\","
        "  \"id\": \"1\","
        "  \"otr\": false,"
        "  \"since_string\": \"Today\","
        "  \"state\": \"COMPLETE\","
        "  \"total\": 128}]"));
  }

 protected:
  scoped_ptr<MockDownloadsDOMHandler> mock_handler_;

 private:
  base::ScopedTempDir downloads_directory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsDOMHandlerTest);
};

// Tests removing all items, both when prohibited and when allowed.
IN_PROC_BROWSER_TEST_F(DownloadsDOMHandlerTest, RemoveAll) {
  DownloadAnItem();

  mock_handler_->reset_downloads_list();
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAllowDeletingBrowserHistory, false);
  mock_handler_->HandleClearAll(NULL);
  // Attempting to clear all shouldn't do anything when deletion is disabled.
  mock_handler_->ForceSendCurrentDownloads();
  mock_handler_->WaitForDownloadsList();
  ASSERT_EQ(1, static_cast<int>(mock_handler_->downloads_list()->GetSize()));

  mock_handler_->reset_downloads_list();
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAllowDeletingBrowserHistory, true);
  mock_handler_->HandleClearAll(NULL);
  mock_handler_->WaitForDownloadsList();
  EXPECT_EQ(0, static_cast<int>(mock_handler_->downloads_list()->GetSize()));
}

// Tests removing one item, both when prohibited and when allowed.
IN_PROC_BROWSER_TEST_F(DownloadsDOMHandlerTest, RemoveOneItem) {
  DownloadAnItem();
  base::ListValue item;
  item.AppendString("1");

  mock_handler_->reset_downloads_list();
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAllowDeletingBrowserHistory, false);
  mock_handler_->HandleRemove(&item);
  // Removing an item only sends the new download list if anything was actually
  // removed, so force it.
  mock_handler_->ForceSendCurrentDownloads();
  mock_handler_->WaitForDownloadsList();
  ASSERT_EQ(1, static_cast<int>(mock_handler_->downloads_list()->GetSize()));

  mock_handler_->reset_downloads_list();
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAllowDeletingBrowserHistory, true);
  mock_handler_->HandleRemove(&item);
  mock_handler_->WaitForDownloadsList();
  EXPECT_EQ(0, static_cast<int>(mock_handler_->downloads_list()->GetSize()));
}

IN_PROC_BROWSER_TEST_F(DownloadsDOMHandlerTest, ClearAllSkipsInProgress) {
  testing::NiceMock<content::MockDownloadManager> manager;
  EXPECT_CALL(manager, GetBrowserContext()).WillRepeatedly(
      testing::Return(browser()->profile()));
  mock_handler_.reset(new MockDownloadsDOMHandler(&manager));
  mock_handler_->HandleGetDownloads(nullptr);

  content::MockDownloadItem item;
  EXPECT_CALL(item, GetState()).WillRepeatedly(
      testing::Return(content::DownloadItem::IN_PROGRESS));
  EXPECT_CALL(item, UpdateObservers()).Times(0);

  std::vector<content::DownloadItem*> items;
  items.push_back(&item);
  EXPECT_CALL(manager, GetAllDownloads(testing::_)).WillOnce(
      testing::SetArgPointee<0>(items));

  mock_handler_->HandleClearAll(NULL);
  EXPECT_TRUE(DownloadItemModel(&item).ShouldShowInShelf());

  mock_handler_.reset();
}

// Tests that DownloadsDOMHandler detects new downloads and relays them to the
// renderer.
// crbug.com/159390: This test fails when daylight savings time ends.
IN_PROC_BROWSER_TEST_F(DownloadsDOMHandlerTest, DownloadsRelayed) {
  DownloadAnItem();

  mock_handler_->WaitForDownloadUpdated();
  const base::DictionaryValue* update = mock_handler_->download_updated();
  ASSERT_TRUE(update);

  bool removed;
  ASSERT_TRUE(update->GetBoolean("file_externally_removed", &removed));
  EXPECT_TRUE(removed);

  std::string id;
  ASSERT_TRUE(update->GetString("id", &id));
  EXPECT_EQ("1", id);

  mock_handler_->reset_downloads_list();
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAllowDeletingBrowserHistory, true);
  mock_handler_->HandleClearAll(NULL);
  mock_handler_->WaitForDownloadsList();
  EXPECT_EQ(0, static_cast<int>(mock_handler_->downloads_list()->GetSize()));
}

// Tests that DownloadsDOMHandler actually calls DownloadItem::Remove() when
// it's closed (and removals can no longer be undone).
IN_PROC_BROWSER_TEST_F(DownloadsDOMHandlerTest, RemoveCalledOnPageClose) {
  testing::NiceMock<content::MockDownloadManager> manager;
  EXPECT_CALL(manager, GetBrowserContext()).WillRepeatedly(
      testing::Return(browser()->profile()));
  mock_handler_.reset(new MockDownloadsDOMHandler(&manager));
  mock_handler_->HandleGetDownloads(nullptr);

  content::MockDownloadItem item;
  EXPECT_CALL(item, GetId()).WillRepeatedly(testing::Return(1));
  EXPECT_CALL(item, GetState()).WillRepeatedly(
      testing::Return(content::DownloadItem::COMPLETE));

  DownloadItemModel model(&item);
  EXPECT_TRUE(model.ShouldShowInShelf());

  EXPECT_CALL(manager, GetDownload(1)).WillRepeatedly(testing::Return(&item));

  base::ListValue remove;
  remove.AppendString("1");
  EXPECT_CALL(item, UpdateObservers()).Times(1);
  mock_handler_->HandleRemove(&remove);
  EXPECT_FALSE(model.ShouldShowInShelf());

  EXPECT_CALL(item, Remove()).Times(1);
  // Call |mock_handler_->FinalizeRemovals()| instead of |mock_handler_.reset()|
  // because the vtable is affected during destruction and the fake manager
  // rigging doesn't work.
  mock_handler_->FinalizeRemovals();
  mock_handler_.reset();
}

// TODO(benjhayden): Test the extension downloads filter for both
// mock_handler_.downloads_list() and mock_handler_.download_updated().

// TODO(benjhayden): Test incognito, both downloads_list() and that on-record
// calls can't access off-record items.

// TODO(benjhayden): Test that bad download ids incoming from the javascript are
// dropped on the floor.

// TODO(benjhayden): Test that IsTemporary() downloads are not shown.

// TODO(benjhayden): Test that RemoveObserver is called on all download items,
// including items that crossed IsTemporary() and back.
