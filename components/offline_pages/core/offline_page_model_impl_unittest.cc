// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_model_impl.h"

#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_storage_manager.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "components/offline_pages/core/offline_page_test_store.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {
const char kOriginalTabNamespace[] = "original_tab_testing_namespace";
const char kTestClientNamespace[] = "default";
const char kUserRequestedNamespace[] = "download";
const GURL kTestUrl("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const GURL kTestUrl3("http://test.xyz");
const GURL kTestUrl4("http://page.net");
const GURL kFileUrl("file:///foo");
const GURL kTestUrlWithFragment("http://example.com#frag");
const GURL kTestUrl2WithFragment("http://other.page.com#frag");
const GURL kTestUrl2WithFragment2("http://other.page.com#frag2");
const ClientId kTestClientId1(kTestClientNamespace, "1234");
const ClientId kTestClientId2(kTestClientNamespace, "5678");
const ClientId kTestClientId3(kTestClientNamespace, "42");
const ClientId kTestUserRequestedClientId(kUserRequestedNamespace, "714");
const int64_t kTestFileSize = 876543LL;
const base::string16 kTestTitle = base::UTF8ToUTF16("a title");
const std::string kRequestOrigin("abc.xyz");

bool URLSpecContains(std::string contains_value, const GURL& url) {
  std::string spec = url.spec();
  return spec.find(contains_value) != std::string::npos;
}

}  // namespace

class OfflinePageModelImplTest
    : public testing::Test,
      public OfflinePageModel::Observer,
      public OfflinePageTestArchiver::Observer,
      public base::SupportsWeakPtr<OfflinePageModelImplTest> {
 public:
  OfflinePageModelImplTest();
  ~OfflinePageModelImplTest() override;

  void SetUp() override;
  void TearDown() override;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageAdded(OfflinePageModel* model,
                        const OfflinePageItem& added_page) override;
  void OfflinePageDeleted(
      const OfflinePageModel::DeletedPageInfo& pageInfo) override;

  // OfflinePageTestArchiver::Observer implementation.
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  // OfflinePageModel callbacks.
  void OnSavePageDone(SavePageResult result, int64_t offline_id);
  void OnAddPageDone(AddPageResult result, int64_t offline_id);
  void OnDeletePageDone(DeletePageResult result);
  void OnCheckPagesExistOfflineDone(const CheckPagesExistOfflineResult& result);
  void OnGetOfflineIdsForClientIdDone(MultipleOfflineIdResult* storage,
                                      const MultipleOfflineIdResult& result);
  void OnGetSingleOfflinePageItemResult(
      std::unique_ptr<OfflinePageItem>* storage,
      const OfflinePageItem* result);
  void OnGetMultipleOfflinePageItemsResult(
      MultipleOfflinePageItemResult* storage,
      const MultipleOfflinePageItemResult& result);
  void OnPagesExpired(bool result);

  // OfflinePageMetadataStore callbacks.
  void OnStoreUpdateDone(bool /* success */);

  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(
      const GURL& url,
      OfflinePageArchiver::ArchiverResult result);
  std::unique_ptr<OfflinePageMetadataStore> BuildStore();
  std::unique_ptr<OfflinePageModelImpl> BuildModel(
      std::unique_ptr<OfflinePageMetadataStore> store);
  void ResetModel();

  // Utility methods.
  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop();
  // Fast-forwards virtual time by |delta|, causing tasks with a remaining
  // delay less than or equal to |delta| to be executed.
  void FastForwardBy(base::TimeDelta delta);
  // Runs tasks and moves time forward until no tasks remain in the queue.
  void FastForwardUntilNoTasksRemain();

  void ResetResults();

  OfflinePageTestStore* GetStore();

  MultipleOfflinePageItemResult GetAllPages();
  MultipleOfflinePageItemResult GetPagesByClientIds(
      const std::vector<ClientId>& client_ids);
  MultipleOfflinePageItemResult GetPagesByRequestOrigin(
      const std::string& origin);
  void DeletePagesByClientIds(const std::vector<ClientId>& client_ids);

  // Saves the page without waiting for it to finish.
  void SavePageWithParamsAsync(
      const OfflinePageModel::SavePageParams& save_page_params,
      std::unique_ptr<OfflinePageArchiver> archiver);

  // Saves the page without waiting for it to finish.
  void SavePageWithArchiverAsync(const GURL& url,
                                 const ClientId& client_id,
                                 const GURL& original_url,
                                 const std::string& request_origin,
                                 std::unique_ptr<OfflinePageArchiver> archiver);

  // All 3 methods below will wait for the save to finish.
  void SavePageWithArchiver(
      const GURL& url,
      const ClientId& client_id,
      std::unique_ptr<OfflinePageArchiver> archiver);
  void SavePageWithArchiverResult(const GURL& url,
                                  const ClientId& client_id,
                                  OfflinePageArchiver::ArchiverResult result);
  // Returns the offline ID of the saved page.
  std::pair<SavePageResult, int64_t> SavePage(const GURL& url,
                                              const ClientId& client_id);
  std::pair<SavePageResult, int64_t> SavePage(
      const GURL& url,
      const ClientId& client_id,
      const std::string& request_origin);

  void AddPage(const OfflinePageItem& offline_page);

  void DeletePage(int64_t offline_id, const DeletePageCallback& callback) {
    std::vector<int64_t> offline_ids;
    offline_ids.push_back(offline_id);
    model()->DeletePagesByOfflineId(offline_ids, callback);
  }

  void SetTemporaryDirSameWithPersistent(bool is_same) {
    temporary_dir_same_with_persistent_ = is_same;
  }

  bool HasPages(std::string name_space);

  CheckPagesExistOfflineResult CheckPagesExistOffline(std::set<GURL>);

  MultipleOfflineIdResult GetOfflineIdsForClientId(const ClientId& client_id);

  std::unique_ptr<OfflinePageItem> GetPageByOfflineId(int64_t offline_id);

  MultipleOfflinePageItemResult GetPagesByFinalURL(const GURL& url);
  MultipleOfflinePageItemResult GetPagesByAllURLS(const GURL& url);

  OfflinePageModelImpl* model() { return model_.get(); }

  int64_t last_save_offline_id() const { return last_save_offline_id_; }

  SavePageResult last_save_result() const { return last_save_result_; }

  AddPageResult last_add_result() const { return last_add_result_; }

  int64_t last_add_offline_id() const { return last_add_offline_id_; }

  DeletePageResult last_delete_result() const { return last_delete_result_; }

  int64_t last_deleted_offline_id() const { return last_deleted_offline_id_; }

  ClientId last_deleted_client_id() const { return last_deleted_client_id_; }

  std::string last_deleted_request_origin() const {
    return last_deleted_request_origin_;
  }

  const base::FilePath& last_archiver_path() { return last_archiver_path_; }

  int last_cleared_pages_count() const { return last_cleared_pages_count_; }

  DeletePageResult last_clear_page_result() const {
    return last_clear_page_result_;
  }

  bool last_expire_page_result() const { return last_expire_page_result_; }

  const base::HistogramTester& histograms() const { return histogram_tester_; }

  const base::FilePath& temporary_dir_path() const {
    if (temporary_dir_same_with_persistent_)
      return persistent_dir_.GetPath();
    return temporary_dir_.GetPath();
  }

  const base::FilePath& persistent_dir_path() const {
    return persistent_dir_.GetPath();
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::ScopedTempDir temporary_dir_;
  base::ScopedTempDir persistent_dir_;

  std::unique_ptr<OfflinePageModelImpl> model_;
  SavePageResult last_save_result_;
  int64_t last_save_offline_id_;
  AddPageResult last_add_result_;
  int64_t last_add_offline_id_;
  DeletePageResult last_delete_result_;
  base::FilePath last_archiver_path_;
  int64_t last_deleted_offline_id_;
  ClientId last_deleted_client_id_;
  std::string last_deleted_request_origin_;
  CheckPagesExistOfflineResult last_pages_exist_result_;
  int last_cleared_pages_count_;
  DeletePageResult last_clear_page_result_;
  bool last_expire_page_result_;
  bool temporary_dir_same_with_persistent_;

  base::HistogramTester histogram_tester_;
};

OfflinePageModelImplTest::OfflinePageModelImplTest()
    : task_runner_(new base::TestMockTimeTaskRunner),
      task_runner_handle_(task_runner_),
      last_save_result_(SavePageResult::CANCELLED),
      last_save_offline_id_(-1),
      last_add_result_(AddPageResult::STORE_FAILURE),
      last_add_offline_id_(-1),
      last_delete_result_(DeletePageResult::CANCELLED),
      last_deleted_offline_id_(-1),
      temporary_dir_same_with_persistent_(false) {}

OfflinePageModelImplTest::~OfflinePageModelImplTest() {}

void OfflinePageModelImplTest::SetUp() {
  ASSERT_TRUE(temporary_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(persistent_dir_.CreateUniqueTempDir());
  model_ = BuildModel(BuildStore());
  model_->GetPolicyController()->AddPolicyForTest(
      kOriginalTabNamespace,
      OfflinePageClientPolicyBuilder(
          kOriginalTabNamespace,
          offline_pages::LifetimePolicy::LifetimeType::TEMPORARY,
          kUnlimitedPages, kUnlimitedPages)
          .SetIsOnlyShownInOriginalTab(true));

  model_->AddObserver(this);
  PumpLoop();
}

void OfflinePageModelImplTest::TearDown() {
  model_->RemoveObserver(this);
  model_.reset();
  PumpLoop();
}

void OfflinePageModelImplTest::OfflinePageModelLoaded(OfflinePageModel* model) {
  ASSERT_EQ(model_.get(), model);
}

void OfflinePageModelImplTest::OfflinePageDeleted(
    const OfflinePageModel::DeletedPageInfo& info) {
  last_deleted_offline_id_ = info.offline_id;
  last_deleted_client_id_ = info.client_id;
  last_deleted_request_origin_ = info.request_origin;
}

void OfflinePageModelImplTest::OfflinePageAdded(
    OfflinePageModel* model,
    const OfflinePageItem& added_page) {
  ASSERT_EQ(model_.get(), model);
}

void OfflinePageModelImplTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {
  last_archiver_path_ = file_path;
}

void OfflinePageModelImplTest::OnSavePageDone(SavePageResult result,
                                              int64_t offline_id) {
  last_save_result_ = result;
  last_save_offline_id_ = offline_id;
}

void OfflinePageModelImplTest::OnAddPageDone(AddPageResult result,
                                             int64_t offline_id) {
  last_add_result_ = result;
  last_add_offline_id_ = offline_id;
}

void OfflinePageModelImplTest::OnDeletePageDone(DeletePageResult result) {
  last_delete_result_ = result;
}

void OfflinePageModelImplTest::OnCheckPagesExistOfflineDone(
    const CheckPagesExistOfflineResult& result) {
  last_pages_exist_result_ = result;
}

void OfflinePageModelImplTest::OnStoreUpdateDone(bool /* success - ignored */) {
}

std::unique_ptr<OfflinePageTestArchiver>
OfflinePageModelImplTest::BuildArchiver(
    const GURL& url,
    OfflinePageArchiver::ArchiverResult result) {
  return std::unique_ptr<OfflinePageTestArchiver>(
      new OfflinePageTestArchiver(this, url, result, kTestTitle, kTestFileSize,
                                  base::ThreadTaskRunnerHandle::Get()));
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageModelImplTest::BuildStore() {
  return std::unique_ptr<OfflinePageMetadataStore>(
      new OfflinePageTestStore(base::ThreadTaskRunnerHandle::Get()));
}

std::unique_ptr<OfflinePageModelImpl> OfflinePageModelImplTest::BuildModel(
    std::unique_ptr<OfflinePageMetadataStore> store) {
  std::unique_ptr<ArchiveManager> archive_manager = nullptr;
  archive_manager = base::MakeUnique<ArchiveManager>(
      temporary_dir_path(), persistent_dir_path(),
      base::ThreadTaskRunnerHandle::Get());
  return std::unique_ptr<OfflinePageModelImpl>(
      new OfflinePageModelImpl(std::move(store), std::move(archive_manager),
                               base::ThreadTaskRunnerHandle::Get()));
}

void OfflinePageModelImplTest::ResetModel() {
  model_->RemoveObserver(this);
  OfflinePageTestStore* old_store = GetStore();
  std::unique_ptr<OfflinePageMetadataStore> new_store(
      new OfflinePageTestStore(*old_store));
  model_ = BuildModel(std::move(new_store));
  model_->AddObserver(this);
  PumpLoop();
}

void OfflinePageModelImplTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void OfflinePageModelImplTest::FastForwardBy(base::TimeDelta delta) {
  task_runner_->FastForwardBy(delta);
}

void OfflinePageModelImplTest::FastForwardUntilNoTasksRemain() {
  task_runner_->FastForwardUntilNoTasksRemain();
}

void OfflinePageModelImplTest::ResetResults() {
  last_save_result_ = SavePageResult::CANCELLED;
  last_delete_result_ = DeletePageResult::CANCELLED;
  last_archiver_path_.clear();
  last_pages_exist_result_.clear();
  last_cleared_pages_count_ = 0;
}

OfflinePageTestStore* OfflinePageModelImplTest::GetStore() {
  return static_cast<OfflinePageTestStore*>(model()->GetStoreForTesting());
}

void OfflinePageModelImplTest::SavePageWithParamsAsync(
    const OfflinePageModel::SavePageParams& save_page_params,
    std::unique_ptr<OfflinePageArchiver> archiver) {
  model()->SavePage(
      save_page_params,
      std::move(archiver),
      base::Bind(&OfflinePageModelImplTest::OnSavePageDone, AsWeakPtr()));
}

void OfflinePageModelImplTest::SavePageWithArchiverAsync(
    const GURL& url,
    const ClientId& client_id,
    const GURL& original_url,
    const std::string& request_origin,
    std::unique_ptr<OfflinePageArchiver> archiver) {
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id = client_id;
  save_page_params.original_url = original_url;
  save_page_params.is_background = false;
  save_page_params.request_origin = request_origin;
  SavePageWithParamsAsync(save_page_params, std::move(archiver));
}

void OfflinePageModelImplTest::SavePageWithArchiver(
    const GURL& url,
    const ClientId& client_id,
    std::unique_ptr<OfflinePageArchiver> archiver) {
  SavePageWithArchiverAsync(url, client_id, GURL(), "", std::move(archiver));
  PumpLoop();
}

void OfflinePageModelImplTest::SavePageWithArchiverResult(
    const GURL& url,
    const ClientId& client_id,
    OfflinePageArchiver::ArchiverResult result) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(url, result));
  SavePageWithArchiverAsync(url, client_id, GURL(), "", std::move(archiver));
  PumpLoop();
}

std::pair<SavePageResult, int64_t>
OfflinePageModelImplTest::SavePage(const GURL& url, const ClientId& client_id) {
  return SavePage(url, client_id, "");
}

std::pair<SavePageResult, int64_t> OfflinePageModelImplTest::SavePage(
    const GURL& url,
    const ClientId& client_id,
    const std::string& request_origin) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      url, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  SavePageWithArchiverAsync(url, client_id, GURL(), request_origin,
                            std::move(archiver));
  PumpLoop();
  return std::make_pair(last_save_result_, last_save_offline_id_);
}

void OfflinePageModelImplTest::AddPage(const OfflinePageItem& offline_page) {
  model()->AddPage(
      offline_page,
      base::Bind(&OfflinePageModelImplTest::OnAddPageDone, AsWeakPtr()));
  PumpLoop();
}

MultipleOfflinePageItemResult OfflinePageModelImplTest::GetAllPages() {
  MultipleOfflinePageItemResult result;
  model()->GetAllPages(
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

MultipleOfflinePageItemResult OfflinePageModelImplTest::GetPagesByClientIds(
    const std::vector<ClientId>& client_ids) {
  MultipleOfflinePageItemResult result;
  model()->GetPagesByClientIds(
      client_ids,
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

MultipleOfflinePageItemResult OfflinePageModelImplTest::GetPagesByRequestOrigin(
    const std::string& origin) {
  MultipleOfflinePageItemResult result;
  model()->GetPagesByRequestOrigin(
      origin,
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

void OfflinePageModelImplTest::DeletePagesByClientIds(
    const std::vector<ClientId>& client_ids) {
  model()->DeletePagesByClientIds(
      client_ids,
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();
}

CheckPagesExistOfflineResult OfflinePageModelImplTest::CheckPagesExistOffline(
    std::set<GURL> pages) {
  model()->CheckPagesExistOffline(
      pages, base::Bind(&OfflinePageModelImplTest::OnCheckPagesExistOfflineDone,
                        AsWeakPtr()));
  PumpLoop();
  return last_pages_exist_result_;
}

MultipleOfflineIdResult OfflinePageModelImplTest::GetOfflineIdsForClientId(
    const ClientId& client_id) {
  MultipleOfflineIdResult result;
  model()->GetOfflineIdsForClientId(
      client_id,
      base::Bind(&OfflinePageModelImplTest::OnGetOfflineIdsForClientIdDone,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

void OfflinePageModelImplTest::OnGetOfflineIdsForClientIdDone(
    MultipleOfflineIdResult* storage,
    const MultipleOfflineIdResult& result) {
  *storage = result;
}

std::unique_ptr<OfflinePageItem> OfflinePageModelImplTest::GetPageByOfflineId(
    int64_t offline_id) {
  std::unique_ptr<OfflinePageItem> result = nullptr;
  model()->GetPageByOfflineId(
      offline_id,
      base::Bind(&OfflinePageModelImplTest::OnGetSingleOfflinePageItemResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

void OfflinePageModelImplTest::OnGetSingleOfflinePageItemResult(
    std::unique_ptr<OfflinePageItem>* storage,
    const OfflinePageItem* result) {
  if (result == nullptr) {
    storage->reset(nullptr);
    return;
  }

  *storage = base::MakeUnique<OfflinePageItem>(*result);
}

void OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult(
    MultipleOfflinePageItemResult* storage,
    const MultipleOfflinePageItemResult& result) {
  *storage = result;
}

void OfflinePageModelImplTest::OnPagesExpired(bool result) {
  last_expire_page_result_ = result;
}

MultipleOfflinePageItemResult OfflinePageModelImplTest::GetPagesByFinalURL(
    const GURL& url) {
  MultipleOfflinePageItemResult result;
  model()->GetPagesByURL(
      url, URLSearchMode::SEARCH_BY_FINAL_URL_ONLY,
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

MultipleOfflinePageItemResult OfflinePageModelImplTest::GetPagesByAllURLS(
    const GURL& url) {
  MultipleOfflinePageItemResult result;
  model()->GetPagesByURL(
      url, URLSearchMode::SEARCH_BY_ALL_URLS,
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&result)));
  PumpLoop();
  return result;
}

bool OfflinePageModelImplTest::HasPages(std::string name_space) {
  MultipleOfflinePageItemResult all_pages = GetAllPages();
  for (const auto& page : all_pages) {
    if (page.client_id.name_space == name_space)
      return true;
  }

  return false;
}

TEST_F(OfflinePageModelImplTest, SavePageSuccessful) {
  EXPECT_FALSE(HasPages(kTestClientNamespace));

  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  SavePageWithArchiverAsync(kTestUrl, kTestClientId1, kTestUrl2, "",
                            std::move(archiver));
  PumpLoop();
  EXPECT_TRUE(HasPages(kTestClientNamespace));

  OfflinePageTestStore* store = GetStore();
  EXPECT_EQ(kTestUrl, store->last_saved_page().url);
  EXPECT_EQ(kTestClientId1.id, store->last_saved_page().client_id.id);
  EXPECT_EQ(kTestClientId1.name_space,
            store->last_saved_page().client_id.name_space);
  // Save last_archiver_path since it will be referred to later.
  base::FilePath archiver_path = last_archiver_path();
  EXPECT_EQ(archiver_path, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  ResetResults();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  ASSERT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestClientId1.id, offline_pages[0].client_id.id);
  EXPECT_EQ(kTestClientId1.name_space, offline_pages[0].client_id.name_space);
  EXPECT_EQ(archiver_path, offline_pages[0].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(0, offline_pages[0].access_count);
  EXPECT_EQ(0, offline_pages[0].flags);
  EXPECT_EQ(kTestTitle, offline_pages[0].title);
  EXPECT_EQ(kTestUrl2, offline_pages[0].original_url);
  EXPECT_EQ("", offline_pages[0].request_origin);
}

TEST_F(OfflinePageModelImplTest, SavePageSuccessfulWithSameOriginalURL) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  // Pass the original URL same as the final URL.
  SavePageWithArchiverAsync(kTestUrl, kTestClientId1, kTestUrl, "",
                            std::move(archiver));
  PumpLoop();

  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  ResetResults();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  ASSERT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  // The original URL should be empty.
  EXPECT_TRUE(offline_pages[0].original_url.is_empty());
}

TEST_F(OfflinePageModelImplTest, SavePageSuccessfulWithRequestOrigin) {
  EXPECT_FALSE(HasPages(kTestClientNamespace));

  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  SavePageWithArchiverAsync(kTestUrl, kTestClientId1, kTestUrl2, kRequestOrigin,
                            std::move(archiver));
  PumpLoop();
  EXPECT_TRUE(HasPages(kTestClientNamespace));

  OfflinePageTestStore* store = GetStore();
  EXPECT_EQ(kTestUrl, store->last_saved_page().url);
  EXPECT_EQ(kTestClientId1.id, store->last_saved_page().client_id.id);
  EXPECT_EQ(kTestClientId1.name_space,
            store->last_saved_page().client_id.name_space);
  // Save last_archiver_path since it will be referred to later.
  base::FilePath archiver_path = last_archiver_path();
  EXPECT_EQ(archiver_path, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  ResetResults();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  ASSERT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestClientId1.id, offline_pages[0].client_id.id);
  EXPECT_EQ(kTestClientId1.name_space, offline_pages[0].client_id.name_space);
  EXPECT_EQ(archiver_path, offline_pages[0].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(0, offline_pages[0].access_count);
  EXPECT_EQ(0, offline_pages[0].flags);
  EXPECT_EQ(kTestTitle, offline_pages[0].title);
  EXPECT_EQ(kTestUrl2, offline_pages[0].original_url);
  EXPECT_EQ(kRequestOrigin, offline_pages[0].request_origin);
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineArchiverCancelled) {
  SavePageWithArchiverResult(
      kTestUrl, kTestClientId1,
      OfflinePageArchiver::ArchiverResult::ERROR_CANCELED);
  EXPECT_EQ(SavePageResult::CANCELLED, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineArchiverDeviceFull) {
  SavePageWithArchiverResult(
      kTestUrl, kTestClientId1,
      OfflinePageArchiver::ArchiverResult::ERROR_DEVICE_FULL);
  EXPECT_EQ(SavePageResult::DEVICE_FULL, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineArchiverContentUnavailable) {
  SavePageWithArchiverResult(
      kTestUrl, kTestClientId1,
      OfflinePageArchiver::ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
  EXPECT_EQ(SavePageResult::CONTENT_UNAVAILABLE, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineCreationFailed) {
  SavePageWithArchiverResult(
      kTestUrl, kTestClientId1,
      OfflinePageArchiver::ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
  EXPECT_EQ(SavePageResult::ARCHIVE_CREATION_FAILED, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineArchiverReturnedWrongUrl) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(GURL("http://other.random.url.com"),
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  SavePageWithArchiver(kTestUrl, kTestClientId1, std::move(archiver));
  EXPECT_EQ(SavePageResult::ARCHIVE_CREATION_FAILED, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineCreationStoreWriteFailure) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::WRITE_FAILED);
  SavePageWithArchiverResult(
      kTestUrl, kTestClientId1,
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED);
  EXPECT_EQ(SavePageResult::STORE_FAILURE, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageLocalFileFailed) {
  // Don't create archiver since it will not be needed for pages that are not
  // going to be saved.
  SavePageWithArchiver(kFileUrl, kTestClientId1,
                       std::unique_ptr<OfflinePageTestArchiver>());
  EXPECT_EQ(SavePageResult::SKIPPED, last_save_result());
}

TEST_F(OfflinePageModelImplTest, SavePageOfflineArchiverTwoPages) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  // archiver_ptr will be valid until after first PumpLoop() call after
  // CompleteCreateArchive() is called.
  OfflinePageTestArchiver* archiver_ptr = archiver.get();
  archiver_ptr->set_delayed(true);
  // First page has no request origin.
  SavePageWithArchiverAsync(kTestUrl, kTestClientId1, GURL(), "",
                            std::move(archiver));
  EXPECT_TRUE(archiver_ptr->create_archive_called());
  // |remove_popup_overlay| should not be turned on on foreground mode.
  EXPECT_FALSE(archiver_ptr->create_archive_params().remove_popup_overlay);

  // Request to save another page, with request origin.
  SavePage(kTestUrl2, kTestClientId2, kRequestOrigin);

  OfflinePageTestStore* store = GetStore();

  EXPECT_EQ(kTestUrl2, store->last_saved_page().url);
  EXPECT_EQ(kTestClientId2, store->last_saved_page().client_id);
  base::FilePath archiver_path2 = last_archiver_path();
  EXPECT_EQ(archiver_path2, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(kRequestOrigin, store->last_saved_page().request_origin);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());

  ResetResults();

  archiver_ptr->CompleteCreateArchive();
  // After this pump loop archiver_ptr is invalid.
  PumpLoop();

  EXPECT_EQ(kTestUrl, store->last_saved_page().url);
  EXPECT_EQ(kTestClientId1, store->last_saved_page().client_id);
  base::FilePath archiver_path = last_archiver_path();
  EXPECT_EQ(archiver_path, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ("", store->last_saved_page().request_origin);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());

  ResetResults();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_EQ(2UL, offline_pages.size());
  // Offline IDs are random, so the order of the pages is also random
  // So load in the right page for the validation below.
  const OfflinePageItem* page1 = nullptr;
  const OfflinePageItem* page2 = nullptr;
  if (offline_pages[0].client_id == kTestClientId1) {
    page1 = &offline_pages[0];
    page2 = &offline_pages[1];
  } else {
    page1 = &offline_pages[1];
    page2 = &offline_pages[0];
  }

  EXPECT_EQ(kTestUrl, page1->url);
  EXPECT_EQ(kTestClientId1, page1->client_id);
  EXPECT_EQ(archiver_path, page1->file_path);
  EXPECT_EQ(kTestFileSize, page1->file_size);
  EXPECT_EQ(0, page1->access_count);
  EXPECT_EQ(0, page1->flags);
  EXPECT_EQ("", page1->request_origin);
  EXPECT_EQ(kTestUrl2, page2->url);
  EXPECT_EQ(kTestClientId2, page2->client_id);
  EXPECT_EQ(archiver_path2, page2->file_path);
  EXPECT_EQ(kTestFileSize, page2->file_size);
  EXPECT_EQ(0, page2->access_count);
  EXPECT_EQ(0, page2->flags);
  EXPECT_EQ(kRequestOrigin, page2->request_origin);
}

TEST_F(OfflinePageModelImplTest, SavePageOnBackground) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  // archiver_ptr will be valid until after first PumpLoop() is called.
  OfflinePageTestArchiver* archiver_ptr = archiver.get();

  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = kTestUrl;
  save_page_params.client_id = kTestClientId1;
  save_page_params.is_background = true;
  save_page_params.use_page_problem_detectors = false;
  SavePageWithParamsAsync(save_page_params, std::move(archiver));
  EXPECT_TRUE(archiver_ptr->create_archive_called());
  // |remove_popup_overlay| should be turned on on background mode.
  EXPECT_TRUE(archiver_ptr->create_archive_params().remove_popup_overlay);

  PumpLoop();
}

TEST_F(OfflinePageModelImplTest, AddPage) {
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temporary_dir_path(), &file_path));
  int64_t offline_id =
      base::RandGenerator(std::numeric_limits<int64_t>::max()) + 1;

  // Adds a fresh page.
  OfflinePageItem offline_page(kTestUrl, offline_id, kTestClientId1, file_path,
                               kTestFileSize);
  offline_page.title = kTestTitle;
  offline_page.original_url = kTestUrl2;
  offline_page.request_origin = kRequestOrigin;
  AddPage(offline_page);

  EXPECT_EQ(AddPageResult::SUCCESS, last_add_result());

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();
  ASSERT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestClientId1, offline_pages[0].client_id);
  EXPECT_EQ(kTestUrl2, offline_pages[0].original_url);
  EXPECT_EQ(kTestTitle, offline_pages[0].title);
  EXPECT_EQ(file_path, offline_pages[0].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(kRequestOrigin, offline_pages[0].request_origin);

  // Trying to adding a same page should result in ALREADY_EXISTS error.
  AddPage(offline_page);
  EXPECT_EQ(AddPageResult::ALREADY_EXISTS, last_add_result());

  //
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::WRITE_FAILED);
  AddPage(offline_page);
  EXPECT_EQ(AddPageResult::STORE_FAILURE, last_add_result());
}

TEST_F(OfflinePageModelImplTest, MarkPageAccessed) {
  SavePage(kTestUrl, kTestClientId1);

  // This will increase access_count by one.
  model()->MarkPageAccessed(last_save_offline_id());
  PumpLoop();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestClientId1, offline_pages[0].client_id);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(1, offline_pages[0].access_count);
}

TEST_F(OfflinePageModelImplTest, GetAllPagesStoreEmpty) {
  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_EQ(0UL, offline_pages.size());
}

TEST_F(OfflinePageModelImplTest, DeletePageSuccessful) {
  OfflinePageTestStore* store = GetStore();

  // Save one page with request origin.
  SavePage(kTestUrl, kTestClientId1, kRequestOrigin);
  int64_t offline1 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(1u, store->GetAllPages().size());

  ResetResults();

  // Save another page, no request origin.
  SavePage(kTestUrl2, kTestClientId2);
  int64_t offline2 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(2u, store->GetAllPages().size());

  ResetResults();

  // Delete one page.
  DeletePage(offline1, base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                  AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline1);
  EXPECT_EQ(last_deleted_client_id(), kTestClientId1);
  EXPECT_EQ(last_deleted_request_origin(), kRequestOrigin);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  ASSERT_EQ(1u, store->GetAllPages().size());
  EXPECT_EQ(kTestUrl2, store->GetAllPages()[0].url);

  // Delete another page.
  DeletePage(offline2, base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                  AsWeakPtr()));

  ResetResults();

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline2);
  EXPECT_EQ(last_deleted_client_id(), kTestClientId2);
  EXPECT_EQ(last_deleted_request_origin(), "");
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(0u, store->GetAllPages().size());
}

TEST_F(OfflinePageModelImplTest, DeleteCachedPageByPredicateUserRequested) {
  OfflinePageTestStore* store = GetStore();

  // Save one page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(1u, store->GetAllPages().size());

  ResetResults();

  // Save an user-requested page in same domain.
  SavePage(kTestUrl, kTestUserRequestedClientId);
  int64_t offline2 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(2u, store->GetAllPages().size());

  ResetResults();

  // Delete the second page.
  model()->DeleteCachedPagesByURLPredicate(
      base::Bind(&URLSpecContains, "example.com"),
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline1);
  EXPECT_EQ(last_deleted_client_id(), kTestClientId1);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  ASSERT_EQ(1u, store->GetAllPages().size());
  EXPECT_EQ(kTestUrl, store->GetAllPages()[0].url);
  EXPECT_EQ(offline2, store->GetAllPages()[0].offline_id);
}

TEST_F(OfflinePageModelImplTest, DeleteCachedPageByPredicate) {
  OfflinePageTestStore* store = GetStore();

  // Save one page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(1u, store->GetAllPages().size());

  ResetResults();

  // Save another page.
  SavePage(kTestUrl2, kTestClientId2);
  int64_t offline2 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(2u, store->GetAllPages().size());

  ResetResults();

  // Delete the second page.
  model()->DeleteCachedPagesByURLPredicate(
      base::Bind(&URLSpecContains, "page.com"),
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline2);
  EXPECT_EQ(last_deleted_client_id(), kTestClientId2);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  ASSERT_EQ(1u, store->GetAllPages().size());
  EXPECT_EQ(kTestUrl, store->GetAllPages()[0].url);

  ResetResults();

  // Delete the first page.
  model()->DeleteCachedPagesByURLPredicate(
      base::Bind(&URLSpecContains, "example.com"),
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline1);
  EXPECT_EQ(last_deleted_client_id(), kTestClientId1);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(0u, store->GetAllPages().size());
}

TEST_F(OfflinePageModelImplTest, DeletePageNotFound) {
  DeletePage(1234LL, base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());

  ResetResults();

  model()->DeleteCachedPagesByURLPredicate(
      base::Bind(&URLSpecContains, "page.com"),
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
}

TEST_F(OfflinePageModelImplTest, DeletePageStoreFailureOnRemove) {
  // Save a page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline_id = last_save_offline_id();
  ResetResults();

  // Try to delete this page.
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::REMOVE_FAILED);
  DeletePage(offline_id, base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                    AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(DeletePageResult::STORE_FAILURE, last_delete_result());
}

TEST_F(OfflinePageModelImplTest, DetectThatOfflineCopyIsMissing) {
  // Save a page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline_id = last_save_offline_id();

  ResetResults();

  std::unique_ptr<OfflinePageItem> page = GetPageByOfflineId(offline_id);

  // Delete the offline copy of the page.
  base::DeleteFile(page->file_path, false);

  // Resetting the model will cause a consistency check.
  ResetModel();

  PumpLoop();

  // Check if the page has been expired.
  EXPECT_EQ(0UL, GetAllPages().size());
}

TEST_F(OfflinePageModelImplTest, DetectThatOfflineCopyIsMissingAfterLoad) {
  // Save a page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline_id = last_save_offline_id();

  ResetResults();

  std::unique_ptr<OfflinePageItem> page = GetPageByOfflineId(offline_id);
  // Delete the offline copy of the page and check the metadata.
  base::DeleteFile(page->file_path, false);
  // Reseting the model should trigger the metadata consistency check as well.
  ResetModel();
  PumpLoop();

  // Check if the page has been expired.
  EXPECT_EQ(0UL, GetAllPages().size());
}

TEST_F(OfflinePageModelImplTest, DetectThatHeadlessPageIsDeleted) {
  // Save a page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline_id = last_save_offline_id();

  ResetResults();
  std::unique_ptr<OfflinePageItem> page = GetPageByOfflineId(offline_id);
  base::FilePath path = page->file_path;
  EXPECT_TRUE(base::PathExists(path));
  GetStore()->ClearAllPages();

  EXPECT_TRUE(base::PathExists(path));
  // Since we've manually changed the store, we have to reload the model to
  // actually refresh the in-memory copy in model. Otherwise GetAllPages()
  // would still have the page we saved above.
  ResetModel();
  PumpLoop();

  EXPECT_EQ(0UL, GetAllPages().size());
  EXPECT_FALSE(base::PathExists(path));
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_DeleteTemporaryPagesInWrongDir \
  DISABLED_DeleteTemporaryPagesInWrongDir
#else
#define MAYBE_DeleteTemporaryPagesInWrongDir DeleteTemporaryPagesInWrongDir
#endif
TEST_F(OfflinePageModelImplTest, MAYBE_DeleteTemporaryPagesInWrongDir) {
  // Set temporary directory same with persistent one.
  SetTemporaryDirSameWithPersistent(true);
  ResetModel();
  PumpLoop();

  // Save a temporary page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t temporary_id = last_save_offline_id();
  // Save a persistent page.
  SavePage(kTestUrl2, kTestUserRequestedClientId);
  int64_t persistent_id = last_save_offline_id();

  EXPECT_EQ(2UL, GetAllPages().size());
  std::unique_ptr<OfflinePageItem> temporary_page =
      GetPageByOfflineId(temporary_id);
  std::unique_ptr<OfflinePageItem> persistent_page =
      GetPageByOfflineId(persistent_id);

  EXPECT_EQ(temporary_page->file_path.DirName(),
            persistent_page->file_path.DirName());

  // Reset model to trigger consistency check, and let temporary directory be
  // different with the persistent one. The previously saved temporary page will
  // be treated as 'saved in persistent location'.
  SetTemporaryDirSameWithPersistent(false);
  ResetModel();
  PumpLoop();

  // Check the temporary page is deleted.
  EXPECT_EQ(1UL, GetAllPages().size());
  EXPECT_EQ(persistent_id, GetAllPages()[0].offline_id);
}

TEST_F(OfflinePageModelImplTest, DeleteMultiplePages) {
  OfflinePageTestStore* store = GetStore();

  // Save 3 pages.
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();

  SavePage(kTestUrl2, kTestClientId2);
  int64_t offline2 = last_save_offline_id();

  SavePage(kTestUrl3, kTestClientId3);
  PumpLoop();

  EXPECT_EQ(3u, store->GetAllPages().size());

  // Delete multiple pages.
  std::vector<int64_t> ids_to_delete;
  ids_to_delete.push_back(offline2);
  ids_to_delete.push_back(offline1);
  ids_to_delete.push_back(23434LL);  // Non-existent ID.
  model()->DeletePagesByOfflineId(
      ids_to_delete,
      base::Bind(&OfflinePageModelImplTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();

  // Success is expected if at least one page is deleted successfully.
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(1u, store->GetAllPages().size());
}

TEST_F(OfflinePageModelImplTest, GetPageByOfflineId) {
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();
  SavePage(kTestUrl2, kTestClientId2);
  int64_t offline2 = last_save_offline_id();

  std::unique_ptr<OfflinePageItem> page = GetPageByOfflineId(offline1);
  ASSERT_TRUE(page);
  EXPECT_EQ(kTestUrl, page->url);
  EXPECT_EQ(kTestClientId1, page->client_id);
  EXPECT_EQ(kTestFileSize, page->file_size);

  page = GetPageByOfflineId(offline2);
  ASSERT_TRUE(page);
  EXPECT_EQ(kTestUrl2, page->url);
  EXPECT_EQ(kTestClientId2, page->client_id);
  EXPECT_EQ(kTestFileSize, page->file_size);

  page = GetPageByOfflineId(-42);
  EXPECT_FALSE(page);
}

TEST_F(OfflinePageModelImplTest, GetPagesByFinalURL) {
  SavePage(kTestUrl, kTestClientId1);
  SavePage(kTestUrl2, kTestClientId2);

  MultipleOfflinePageItemResult pages = GetPagesByFinalURL(kTestUrl2);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl2, pages[0].url);
  EXPECT_EQ(kTestClientId2, pages[0].client_id);

  pages = GetPagesByFinalURL(kTestUrl);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl, pages[0].url);
  EXPECT_EQ(kTestClientId1, pages[0].client_id);

  pages = GetPagesByFinalURL(GURL("http://foo"));
  EXPECT_EQ(0U, pages.size());
}

TEST_F(OfflinePageModelImplTest, GetPagesByFinalURLWithFragmentStripped) {
  SavePage(kTestUrl, kTestClientId1);
  SavePage(kTestUrl2WithFragment, kTestClientId2);

  MultipleOfflinePageItemResult pages =
      GetPagesByFinalURL(kTestUrlWithFragment);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl, pages[0].url);
  EXPECT_EQ(kTestClientId1, pages[0].client_id);

  pages = GetPagesByFinalURL(kTestUrl2);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl2WithFragment, pages[0].url);
  EXPECT_EQ(kTestClientId2, pages[0].client_id);

  pages = GetPagesByFinalURL(kTestUrl2WithFragment2);
  EXPECT_EQ(1U, pages.size());
  EXPECT_EQ(kTestUrl2WithFragment, pages[0].url);
  EXPECT_EQ(kTestClientId2, pages[0].client_id);
}

TEST_F(OfflinePageModelImplTest, GetPagesByAllURLS) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  SavePageWithArchiverAsync(kTestUrl, kTestClientId1, kTestUrl2, "",
                            std::move(archiver));
  PumpLoop();

  SavePage(kTestUrl2, kTestClientId2);

  MultipleOfflinePageItemResult pages = GetPagesByAllURLS(kTestUrl2);
  ASSERT_EQ(2U, pages.size());
  // Validates the items regardless their order.
  int i = -1;
  if (pages[0].url == kTestUrl2)
    i = 0;
  else if (pages[1].url == kTestUrl2)
    i = 1;
  ASSERT_NE(-1, i);
  EXPECT_EQ(kTestUrl2, pages[i].url);
  EXPECT_EQ(kTestClientId2, pages[i].client_id);
  EXPECT_EQ(GURL(), pages[i].original_url);
  EXPECT_EQ(kTestUrl, pages[1 - i].url);
  EXPECT_EQ(kTestClientId1, pages[1 - i].client_id);
  EXPECT_EQ(kTestUrl2, pages[1 - i].original_url);
}

TEST_F(OfflinePageModelImplTest, CheckPagesExistOffline) {
  SavePage(kTestUrl, kTestClientId1);
  SavePage(kTestUrl2, kTestClientId2);

  const ClientId last_n_client_id(kOriginalTabNamespace, "1234");
  SavePage(kTestUrl3, last_n_client_id);

  std::set<GURL> input;
  input.insert(kTestUrl);
  input.insert(kTestUrl2);
  input.insert(kTestUrl3);
  input.insert(kTestUrl4);

  CheckPagesExistOfflineResult existing_pages = CheckPagesExistOffline(input);
  EXPECT_EQ(2U, existing_pages.size());
  EXPECT_NE(existing_pages.end(), existing_pages.find(kTestUrl));
  EXPECT_NE(existing_pages.end(), existing_pages.find(kTestUrl2));
  EXPECT_EQ(existing_pages.end(), existing_pages.find(kTestUrl3));
  EXPECT_EQ(existing_pages.end(), existing_pages.find(kTestUrl4));
}

TEST_F(OfflinePageModelImplTest, CanSaveURL) {
  EXPECT_TRUE(OfflinePageModel::CanSaveURL(GURL("http://foo")));
  EXPECT_TRUE(OfflinePageModel::CanSaveURL(GURL("https://foo")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("file:///foo")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("data:image/png;base64,ab")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("chrome://version")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("chrome-native://newtab/")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("/invalid/url.mhtml")));
}

TEST_F(OfflinePageModelImplTest, SaveRetrieveMultipleClientIds) {
  EXPECT_FALSE(HasPages(kTestClientNamespace));
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_TRUE(HasPages(kTestClientNamespace));

  SavePage(kTestUrl2, kTestClientId1);
  int64_t offline2 = last_save_offline_id();

  EXPECT_NE(offline1, offline2);

  const std::vector<int64_t> ids = GetOfflineIdsForClientId(kTestClientId1);

  EXPECT_EQ(2UL, ids.size());

  std::set<int64_t> id_set;
  for (size_t i = 0; i < ids.size(); i++) {
    id_set.insert(ids[i]);
  }

  EXPECT_TRUE(id_set.find(offline1) != id_set.end());
  EXPECT_TRUE(id_set.find(offline2) != id_set.end());
}

TEST_F(OfflinePageModelImplTest, SaveMultiplePagesWithSameURLBySameClientId) {
  EXPECT_FALSE(HasPages(kTestClientNamespace));
  SavePage(kTestUrl, kTestClientId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_TRUE(HasPages(kTestClientNamespace));

  SavePage(kTestUrl, kTestClientId1);
  int64_t offline2 = last_save_offline_id();

  EXPECT_NE(offline1, offline2);

  const std::vector<int64_t> ids = GetOfflineIdsForClientId(kTestClientId1);

  EXPECT_EQ(1UL, ids.size());

  std::set<int64_t> id_set;
  for (size_t i = 0; i < ids.size(); i++) {
    id_set.insert(ids[i]);
  }

  EXPECT_TRUE(id_set.find(offline2) != id_set.end());
}

TEST_F(OfflinePageModelImplTest, DownloadMetrics) {
  EXPECT_FALSE(HasPages(kUserRequestedNamespace));
  SavePage(kTestUrl, kTestUserRequestedClientId);
  histograms().ExpectUniqueSample(
      "OfflinePages.DownloadSavedPageDuplicateCount", 1, 1);
  FastForwardBy(base::TimeDelta::FromMinutes(1));
  histograms().ExpectTotalCount(
      "OfflinePages.DownloadSavedPageTimeSinceDuplicateSaved", 0);
  SavePage(kTestUrl, kTestUserRequestedClientId);
  histograms().ExpectTotalCount("OfflinePages.DownloadSavedPageDuplicateCount",
                                2);
  histograms().ExpectBucketCount("OfflinePages.DownloadSavedPageDuplicateCount",
                                 2, 1);
  histograms().ExpectBucketCount("OfflinePages.DownloadSavedPageDuplicateCount",
                                 1, 1);
  histograms().ExpectTotalCount(
      "OfflinePages.DownloadSavedPageTimeSinceDuplicateSaved", 1);
  histograms().ExpectTotalCount(
      "OfflinePages.DownloadDeletedPageDuplicateCount", 0);

  // void DeletePage(int64_t offline_id, const DeletePageCallback& callback) {
  const std::vector<int64_t> ids =
      GetOfflineIdsForClientId(kTestUserRequestedClientId);
  ASSERT_EQ(2U, ids.size());

  DeletePage(ids[0], base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                AsWeakPtr()));
  PumpLoop();
  histograms().ExpectUniqueSample(
      "OfflinePages.DownloadDeletedPageDuplicateCount", 2, 1);
  DeletePage(ids[1], base::Bind(&OfflinePageModelImplTest::OnDeletePageDone,
                                AsWeakPtr()));
  PumpLoop();
  // No change when we delete the last page.
  histograms().ExpectTotalCount(
      "OfflinePages.DownloadDeletedPageDuplicateCount", 2);
  histograms().ExpectBucketCount(
      "OfflinePages.DownloadDeletedPageDuplicateCount", 1, 1);
  histograms().ExpectBucketCount(
      "OfflinePages.DownloadDeletedPageDuplicateCount", 2, 1);
}

TEST_F(OfflinePageModelImplTest, GetPagesByClientIds) {
  // We will save 2 pages.
  std::pair<SavePageResult, int64_t> saved_pages[3];
  saved_pages[0] = SavePage(kTestUrl, kTestClientId1);
  saved_pages[1] = SavePage(kTestUrl2, kTestClientId2);

  for (const auto& save_result : saved_pages) {
    ASSERT_EQ(OfflinePageModel::SavePageResult::SUCCESS,
              std::get<0>(save_result));
  }

  std::vector<ClientId> client_ids = {kTestClientId2};
  std::vector<OfflinePageItem> offline_pages = GetPagesByClientIds(client_ids);
  EXPECT_EQ(1U, offline_pages.size());

  const OfflinePageItem& item = offline_pages[0];
  EXPECT_EQ(kTestClientId2.name_space, item.client_id.name_space);
  EXPECT_EQ(kTestClientId2.id, item.client_id.id);
  EXPECT_EQ(kTestUrl2, item.url);
}

TEST_F(OfflinePageModelImplTest, GetPagesByRequestOrigin) {
  // We will save 3 pages.
  std::string origin1("abc.xyz");
  std::string origin2("abc");
  std::pair<SavePageResult, int64_t> save_pages[3];
  save_pages[0] = SavePage(kTestUrl, kTestClientId1, origin1);
  save_pages[1] = SavePage(kTestUrl2, kTestClientId2, origin2);
  save_pages[2] = SavePage(kTestUrl3, kTestClientId3, origin1);

  for (const auto& save_result : save_pages) {
    ASSERT_EQ(OfflinePageModel::SavePageResult::SUCCESS,
              std::get<0>(save_result));
  }

  std::vector<OfflinePageItem> offline_pages = GetPagesByRequestOrigin(origin2);
  EXPECT_EQ(1U, offline_pages.size());

  const OfflinePageItem& item = offline_pages[0];
  EXPECT_EQ(kTestUrl2, item.url);
  EXPECT_EQ(origin2, item.request_origin);
  EXPECT_EQ(kTestClientId2.name_space, item.client_id.name_space);
  EXPECT_EQ(kTestClientId2.id, item.client_id.id);
}

TEST_F(OfflinePageModelImplTest, DeletePagesByClientIds) {
  // We will save 3 pages.
  std::pair<SavePageResult, int64_t> saved_pages[3];
  saved_pages[0] = SavePage(kTestUrl, kTestClientId1);
  saved_pages[1] = SavePage(kTestUrl2, kTestClientId2);
  saved_pages[2] = SavePage(kTestUrl3, kTestClientId3);

  for (const auto& save_result : saved_pages) {
    ASSERT_EQ(OfflinePageModel::SavePageResult::SUCCESS,
              std::get<0>(save_result));
  }

  std::vector<ClientId> client_ids = {kTestClientId1, kTestClientId2};
  DeletePagesByClientIds(client_ids);
  std::vector<OfflinePageItem> offline_pages = GetAllPages();
  ASSERT_EQ(1U, offline_pages.size());

  const OfflinePageItem& item = offline_pages[0];
  EXPECT_EQ(kTestClientId3.name_space, item.client_id.name_space);
  EXPECT_EQ(kTestClientId3.id, item.client_id.id);
  EXPECT_EQ(kTestUrl3, item.url);
}

TEST_F(OfflinePageModelImplTest, CustomTabsNamespace) {
  SavePage(kTestUrl, ClientId(kCCTNamespace, "123"));
  std::string histogram_name = "OfflinePages.SavePageResult.";
  histogram_name += kCCTNamespace;

  histograms().ExpectUniqueSample(histogram_name,
                                  static_cast<int>(SavePageResult::SUCCESS), 1);
}

TEST_F(OfflinePageModelImplTest, DownloadNamespace) {
  SavePage(kTestUrl, ClientId(kDownloadNamespace, "123"));
  std::string histogram_name = "OfflinePages.SavePageResult.";
  histogram_name += kDownloadNamespace;

  histograms().ExpectUniqueSample(histogram_name,
                                  static_cast<int>(SavePageResult::SUCCESS), 1);
}

TEST_F(OfflinePageModelImplTest, NewTabPageNamespace) {
  SavePage(kTestUrl, ClientId(kNTPSuggestionsNamespace, "123"));
  std::string histogram_name = "OfflinePages.SavePageResult.";
  histogram_name += kNTPSuggestionsNamespace;

  histograms().ExpectUniqueSample(histogram_name,
                                  static_cast<int>(SavePageResult::SUCCESS), 1);
}

TEST_F(OfflinePageModelImplTest, StoreLoadFailurePersists) {
  // Initial database load is successful. Only takes 1 attempt.
  EXPECT_EQ(1, GetStore()->initialize_attempts_count());
  histograms().ExpectUniqueSample("OfflinePages.Model.FinalLoadSuccessful",
                                  true, 1);
  histograms().ExpectUniqueSample("OfflinePages.Model.InitAttemptsSpent", 1, 1);

  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::LOAD_FAILED_RESET_SUCCESS);
  ResetModel();
  // Skip all the retries with delays.
  FastForwardUntilNoTasksRemain();
  // All available attempts were spent.
  EXPECT_EQ(3, GetStore()->initialize_attempts_count());

  // Should record failure to load.
  histograms().ExpectBucketCount("OfflinePages.Model.FinalLoadSuccessful",
                                 false, 1);
  // Should show the previous count since no attempts are recorded for
  // failure. In case of failure, all attempts are assumed spent.
  histograms().ExpectUniqueSample("OfflinePages.Model.InitAttemptsSpent", 1, 1);

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  // Model will 'load' but the store underneath it is not functional and
  // will silently fail all sql operations.
  EXPECT_TRUE(model()->is_loaded());
  EXPECT_EQ(StoreState::FAILED_LOADING, GetStore()->state());
  EXPECT_EQ(0UL, offline_pages.size());

  // The pages can still be saved, they will not be persisted to disk.
  // Verify no crashes and error code returned.
  std::pair<SavePageResult, int64_t> result =
      SavePage(kTestUrl, ClientId(kDownloadNamespace, "123"));
  EXPECT_EQ(SavePageResult::STORE_FAILURE, result.first);
}

TEST_F(OfflinePageModelImplTest, GetPagesByNamespace) {
  SavePage(kTestUrl, ClientId(kCCTNamespace, "123"));
  SavePage(kTestUrl, ClientId(kDownloadNamespace, "456"));
  base::FilePath archive_path(last_archiver_path());
  SavePage(kTestUrl, ClientId(kNTPSuggestionsNamespace, "789"));

  MultipleOfflinePageItemResult offline_pages;
  model()->GetPagesByNamespace(
      kDownloadNamespace,
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&offline_pages)));
  PumpLoop();

  ASSERT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ("456", offline_pages[0].client_id.id);
  EXPECT_EQ(kDownloadNamespace, offline_pages[0].client_id.name_space);
  EXPECT_EQ(archive_path, offline_pages[0].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(0, offline_pages[0].access_count);
  EXPECT_EQ(0, offline_pages[0].flags);
  EXPECT_EQ(kTestTitle, offline_pages[0].title);
  EXPECT_EQ(GURL(), offline_pages[0].original_url);
  EXPECT_EQ("", offline_pages[0].request_origin);
}

TEST_F(OfflinePageModelImplTest, GetPagesRemovedOnCacheReset) {
  SavePage(kTestUrl, ClientId(kCCTNamespace, "123"));
  base::FilePath archive_path(last_archiver_path());
  SavePage(kTestUrl, ClientId(kDownloadNamespace, "456"));
  SavePage(kTestUrl, ClientId(kNTPSuggestionsNamespace, "789"));

  MultipleOfflinePageItemResult offline_pages;
  model()->GetPagesRemovedOnCacheReset(
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&offline_pages)));
  PumpLoop();

  ASSERT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ("123", offline_pages[0].client_id.id);
  EXPECT_EQ(kCCTNamespace, offline_pages[0].client_id.name_space);
  EXPECT_EQ(archive_path, offline_pages[0].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(0, offline_pages[0].access_count);
  EXPECT_EQ(0, offline_pages[0].flags);
  EXPECT_EQ(kTestTitle, offline_pages[0].title);
  EXPECT_EQ(GURL(), offline_pages[0].original_url);
  EXPECT_EQ("", offline_pages[0].request_origin);
}

TEST_F(OfflinePageModelImplTest, GetPagesSupportedByDownloads) {
  SavePage(kTestUrl, ClientId(kCCTNamespace, "123"));
  SavePage(kTestUrl, ClientId(kDownloadNamespace, "456"));
  base::FilePath download_archive_path(last_archiver_path());
  SavePage(kTestUrl, ClientId(kNTPSuggestionsNamespace, "789"));
  base::FilePath ntp_suggestions_archive_path(last_archiver_path());

  MultipleOfflinePageItemResult offline_pages;
  model()->GetPagesSupportedByDownloads(
      base::Bind(&OfflinePageModelImplTest::OnGetMultipleOfflinePageItemsResult,
                 AsWeakPtr(), base::Unretained(&offline_pages)));
  PumpLoop();

  ASSERT_EQ(2UL, offline_pages.size());
  int download_index = 0;
  int ntp_suggestions_index = 1;
  if (offline_pages[0].client_id.name_space != kDownloadNamespace) {
    download_index = 1;
    ntp_suggestions_index = 0;
  }

  EXPECT_EQ(kTestUrl, offline_pages[download_index].url);
  EXPECT_EQ("456", offline_pages[download_index].client_id.id);
  EXPECT_EQ(kDownloadNamespace,
            offline_pages[download_index].client_id.name_space);
  EXPECT_EQ(download_archive_path, offline_pages[download_index].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[download_index].file_size);
  EXPECT_EQ(0, offline_pages[download_index].access_count);
  EXPECT_EQ(0, offline_pages[download_index].flags);
  EXPECT_EQ(kTestTitle, offline_pages[download_index].title);
  EXPECT_EQ(GURL(), offline_pages[download_index].original_url);
  EXPECT_EQ("", offline_pages[download_index].request_origin);

  EXPECT_EQ(kTestUrl, offline_pages[ntp_suggestions_index].url);
  EXPECT_EQ("789", offline_pages[ntp_suggestions_index].client_id.id);
  EXPECT_EQ(kNTPSuggestionsNamespace,
            offline_pages[ntp_suggestions_index].client_id.name_space);
  EXPECT_EQ(ntp_suggestions_archive_path,
            offline_pages[ntp_suggestions_index].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[ntp_suggestions_index].file_size);
  EXPECT_EQ(0, offline_pages[ntp_suggestions_index].access_count);
  EXPECT_EQ(0, offline_pages[ntp_suggestions_index].flags);
  EXPECT_EQ(kTestTitle, offline_pages[ntp_suggestions_index].title);
  EXPECT_EQ(GURL(), offline_pages[ntp_suggestions_index].original_url);
  EXPECT_EQ("", offline_pages[ntp_suggestions_index].request_origin);
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_CheckPagesSavedInSeparateDirs \
  DISABLED_CheckPagesSavedInSeparateDirs
#else
#define MAYBE_CheckPagesSavedInSeparateDirs CheckPagesSavedInSeparateDirs
#endif
TEST_F(OfflinePageModelImplTest, MAYBE_CheckPagesSavedInSeparateDirs) {
  // Save a temporary page.
  SavePage(kTestUrl, kTestClientId1);
  int64_t temporary_id = last_save_offline_id();
  // Save a persistent page.
  SavePage(kTestUrl2, kTestUserRequestedClientId);
  int64_t persistent_id = last_save_offline_id();

  std::unique_ptr<OfflinePageItem> temporary_page =
      GetPageByOfflineId(temporary_id);
  std::unique_ptr<OfflinePageItem> persistent_page =
      GetPageByOfflineId(persistent_id);

  ASSERT_TRUE(temporary_page);
  ASSERT_TRUE(persistent_page);

  base::FilePath temporary_page_path = temporary_page->file_path;
  base::FilePath persistent_page_path = persistent_page->file_path;

  EXPECT_TRUE(temporary_dir_path().IsParent(temporary_page_path));
  EXPECT_TRUE(persistent_dir_path().IsParent(persistent_page_path));
  EXPECT_NE(temporary_page_path.DirName(), persistent_page_path.DirName());
}

}  // namespace offline_pages
