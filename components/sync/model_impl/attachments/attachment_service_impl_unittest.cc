// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/attachments/attachment_service_impl.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/mock_timer.h"
#include "components/sync/engine/attachments/attachment_store_backend.h"
#include "components/sync/engine/attachments/attachment_util.h"
#include "components/sync/engine/attachments/fake_attachment_downloader.h"
#include "components/sync/engine/attachments/fake_attachment_uploader.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class MockAttachmentStoreBackend
    : public AttachmentStoreBackend,
      public base::SupportsWeakPtr<MockAttachmentStoreBackend> {
 public:
  MockAttachmentStoreBackend(
      const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner)
      : AttachmentStoreBackend(callback_task_runner) {}

  ~MockAttachmentStoreBackend() override {}

  void Init(const AttachmentStore::InitCallback& callback) override {}

  void Read(AttachmentStore::Component component,
            const AttachmentIdList& ids,
            const AttachmentStore::ReadCallback& callback) override {
    read_ids.push_back(ids);
    read_callbacks.push_back(callback);
  }

  void Write(AttachmentStore::Component component,
             const AttachmentList& attachments,
             const AttachmentStore::WriteCallback& callback) override {
    write_attachments.push_back(attachments);
    write_callbacks.push_back(callback);
  }

  void SetReference(AttachmentStore::Component component,
                    const AttachmentIdList& ids) override {
    set_reference_ids.push_back(std::make_pair(component, ids));
  }

  void DropReference(AttachmentStore::Component component,
                     const AttachmentIdList& ids,
                     const AttachmentStore::DropCallback& callback) override {
    ASSERT_EQ(AttachmentStore::SYNC, component);
    drop_ids.push_back(ids);
  }

  void ReadMetadataById(
      AttachmentStore::Component component,
      const AttachmentIdList& ids,
      const AttachmentStore::ReadMetadataCallback& callback) override {
    NOTREACHED();
  }

  void ReadMetadata(
      AttachmentStore::Component component,
      const AttachmentStore::ReadMetadataCallback& callback) override {
    NOTREACHED();
  }

  // Respond to Read request. Attachments found in local_attachments should be
  // returned, everything else should be reported unavailable.
  void RespondToRead(const AttachmentIdSet& local_attachments) {
    scoped_refptr<base::RefCountedString> data = new base::RefCountedString();
    AttachmentStore::ReadCallback callback = read_callbacks.back();
    AttachmentIdList ids = read_ids.back();
    read_callbacks.pop_back();
    read_ids.pop_back();

    std::unique_ptr<AttachmentMap> attachments(new AttachmentMap());
    std::unique_ptr<AttachmentIdList> unavailable_attachments(
        new AttachmentIdList());
    for (AttachmentIdList::const_iterator iter = ids.begin(); iter != ids.end();
         ++iter) {
      if (local_attachments.find(*iter) != local_attachments.end()) {
        Attachment attachment = Attachment::CreateFromParts(*iter, data);
        attachments->insert(std::make_pair(*iter, attachment));
      } else {
        unavailable_attachments->push_back(*iter);
      }
    }
    AttachmentStore::Result result = unavailable_attachments->empty()
                                         ? AttachmentStore::SUCCESS
                                         : AttachmentStore::UNSPECIFIED_ERROR;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, result, base::Passed(&attachments),
                              base::Passed(&unavailable_attachments)));
  }

  // Respond to Write request with |result|.
  void RespondToWrite(const AttachmentStore::Result& result) {
    AttachmentStore::WriteCallback callback = write_callbacks.back();
    write_callbacks.pop_back();
    write_attachments.pop_back();
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, result));
  }

  std::vector<AttachmentIdList> read_ids;
  std::vector<AttachmentStore::ReadCallback> read_callbacks;
  std::vector<AttachmentList> write_attachments;
  std::vector<AttachmentStore::WriteCallback> write_callbacks;
  std::vector<std::pair<AttachmentStore::Component, AttachmentIdList>>
      set_reference_ids;
  std::vector<AttachmentIdList> drop_ids;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAttachmentStoreBackend);
};

class MockAttachmentDownloader
    : public AttachmentDownloader,
      public base::SupportsWeakPtr<MockAttachmentDownloader> {
 public:
  MockAttachmentDownloader() {}

  void DownloadAttachment(const AttachmentId& id,
                          const DownloadCallback& callback) override {
    ASSERT_TRUE(download_requests.find(id) == download_requests.end());
    download_requests.insert(std::make_pair(id, callback));
  }

  // Multiple requests to download will be active at the same time.
  // RespondToDownload should respond to only one of them.
  void RespondToDownload(const AttachmentId& id, const DownloadResult& result) {
    ASSERT_TRUE(download_requests.find(id) != download_requests.end());
    std::unique_ptr<Attachment> attachment;
    if (result == DOWNLOAD_SUCCESS) {
      scoped_refptr<base::RefCountedString> data = new base::RefCountedString();
      attachment =
          base::MakeUnique<Attachment>(Attachment::CreateFromParts(id, data));
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(download_requests[id], result, base::Passed(&attachment)));

    download_requests.erase(id);
  }

  std::map<AttachmentId, DownloadCallback> download_requests;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAttachmentDownloader);
};

class MockAttachmentUploader
    : public AttachmentUploader,
      public base::SupportsWeakPtr<MockAttachmentUploader> {
 public:
  MockAttachmentUploader() {}

  // AttachmentUploader implementation.
  void UploadAttachment(const Attachment& attachment,
                        const UploadCallback& callback) override {
    const AttachmentId id = attachment.GetId();
    ASSERT_TRUE(upload_requests.find(id) == upload_requests.end());
    upload_requests.insert(std::make_pair(id, callback));
  }

  void RespondToUpload(const AttachmentId& id, const UploadResult& result) {
    ASSERT_TRUE(upload_requests.find(id) != upload_requests.end());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(upload_requests[id], result, id));
    upload_requests.erase(id);
  }

  std::map<AttachmentId, UploadCallback> upload_requests;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAttachmentUploader);
};

}  // namespace

class AttachmentServiceImplTest : public testing::Test,
                                  public AttachmentService::Delegate {
 protected:
  AttachmentServiceImplTest() {}

  void SetUp() override {
    network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());
    InitializeAttachmentService(base::MakeUnique<MockAttachmentUploader>(),
                                base::MakeUnique<MockAttachmentDownloader>(),
                                this);
  }

  void TearDown() override {
    attachment_service_.reset();
    RunLoop();
    ASSERT_FALSE(attachment_store_backend_);
    ASSERT_FALSE(attachment_uploader_);
    ASSERT_FALSE(attachment_downloader_);
  }

  // AttachmentService::Delegate implementation.
  void OnAttachmentUploaded(const AttachmentId& attachment_id) override {
    on_attachment_uploaded_list_.push_back(attachment_id);
  }

  void InitializeAttachmentService(
      std::unique_ptr<MockAttachmentUploader> uploader,
      std::unique_ptr<MockAttachmentDownloader> downloader,
      AttachmentService::Delegate* delegate) {
    // Initialize mock attachment store
    scoped_refptr<base::SingleThreadTaskRunner> runner =
        base::ThreadTaskRunnerHandle::Get();
    std::unique_ptr<MockAttachmentStoreBackend> attachment_store_backend(
        new MockAttachmentStoreBackend(runner));
    attachment_store_backend_ = attachment_store_backend->AsWeakPtr();
    std::unique_ptr<AttachmentStore> attachment_store =
        AttachmentStore::CreateMockStoreForTest(
            std::move(attachment_store_backend));

    if (uploader.get()) {
      attachment_uploader_ = uploader->AsWeakPtr();
    }
    if (downloader.get()) {
      attachment_downloader_ = downloader->AsWeakPtr();
    }
    attachment_service_ = base::MakeUnique<AttachmentServiceImpl>(
        attachment_store->CreateAttachmentStoreForSync(), std::move(uploader),
        std::move(downloader), delegate, base::TimeDelta::FromMinutes(1),
        base::TimeDelta::FromMinutes(8));

    std::unique_ptr<base::MockTimer> timer_to_pass(
        new base::MockTimer(false, false));
    mock_timer_ = timer_to_pass.get();
    attachment_service_->SetTimerForTest(std::move(timer_to_pass));
  }

  AttachmentService* attachment_service() { return attachment_service_.get(); }

  base::MockTimer* mock_timer() { return mock_timer_; }

  AttachmentService::GetOrDownloadCallback download_callback() {
    return base::Bind(&AttachmentServiceImplTest::DownloadDone,
                      base::Unretained(this));
  }

  void DownloadDone(const AttachmentService::GetOrDownloadResult& result,
                    std::unique_ptr<AttachmentMap> attachments) {
    download_results_.push_back(result);
    last_download_attachments_ = std::move(attachments);
  }

  void RunLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void RunLoopAndFireTimer() {
    RunLoop();
    if (mock_timer()->IsRunning()) {
      mock_timer()->Fire();
      RunLoop();
    }
  }

  static AttachmentIdSet AttachmentIdSetFromList(
      const AttachmentIdList& id_list) {
    AttachmentIdSet id_set;
    std::copy(id_list.begin(), id_list.end(),
              std::inserter(id_set, id_set.end()));
    return id_set;
  }

  const std::vector<AttachmentService::GetOrDownloadResult>& download_results()
      const {
    return download_results_;
  }

  const AttachmentMap& last_download_attachments() const {
    return *last_download_attachments_.get();
  }

  net::NetworkChangeNotifier* network_change_notifier() {
    return network_change_notifier_.get();
  }

  MockAttachmentStoreBackend* store() {
    return attachment_store_backend_.get();
  }

  MockAttachmentDownloader* downloader() {
    return attachment_downloader_.get();
  }

  MockAttachmentUploader* uploader() { return attachment_uploader_.get(); }

  const std::vector<AttachmentId>& on_attachment_uploaded_list() const {
    return on_attachment_uploaded_list_;
  }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  base::WeakPtr<MockAttachmentStoreBackend> attachment_store_backend_;
  base::WeakPtr<MockAttachmentDownloader> attachment_downloader_;
  base::WeakPtr<MockAttachmentUploader> attachment_uploader_;
  std::unique_ptr<AttachmentServiceImpl> attachment_service_;
  base::MockTimer* mock_timer_;  // not owned

  std::vector<AttachmentService::GetOrDownloadResult> download_results_;
  std::unique_ptr<AttachmentMap> last_download_attachments_;
  std::vector<AttachmentId> on_attachment_uploaded_list_;
};

TEST_F(AttachmentServiceImplTest, GetOrDownload_EmptyAttachmentList) {
  AttachmentIdList attachment_ids;
  attachment_service()->GetOrDownloadAttachments(attachment_ids,
                                                 download_callback());
  RunLoop();
  store()->RespondToRead(AttachmentIdSet());

  RunLoop();
  EXPECT_EQ(1U, download_results().size());
  EXPECT_EQ(0U, last_download_attachments().size());
}

TEST_F(AttachmentServiceImplTest, GetOrDownload_Local) {
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create(0, 0));
  attachment_service()->GetOrDownloadAttachments(attachment_ids,
                                                 download_callback());
  AttachmentIdSet local_attachments;
  local_attachments.insert(attachment_ids[0]);
  RunLoop();
  EXPECT_EQ(1U, store()->set_reference_ids.size());
  EXPECT_EQ(AttachmentStore::MODEL_TYPE, store()->set_reference_ids[0].first);
  store()->RespondToRead(local_attachments);

  RunLoop();
  EXPECT_EQ(1U, download_results().size());
  EXPECT_EQ(1U, last_download_attachments().size());
  EXPECT_TRUE(last_download_attachments().find(attachment_ids[0]) !=
              last_download_attachments().end());
}

TEST_F(AttachmentServiceImplTest, GetOrDownload_LocalRemoteUnavailable) {
  // Create attachment list with 4 ids.
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create(0, 0));
  attachment_ids.push_back(AttachmentId::Create(0, 0));
  attachment_ids.push_back(AttachmentId::Create(0, 0));
  attachment_ids.push_back(AttachmentId::Create(0, 0));
  // Call attachment service.
  attachment_service()->GetOrDownloadAttachments(attachment_ids,
                                                 download_callback());
  RunLoop();
  // Ensure AttachmentStore is called.
  EXPECT_FALSE(store()->read_ids.empty());

  // Make AttachmentStore return only attachment 0.
  AttachmentIdSet local_attachments;
  local_attachments.insert(attachment_ids[0]);
  store()->RespondToRead(local_attachments);
  RunLoop();
  // Ensure Downloader called with right attachment ids
  EXPECT_EQ(3U, downloader()->download_requests.size());

  // Make downloader return attachment 1.
  downloader()->RespondToDownload(attachment_ids[1],
                                  AttachmentDownloader::DOWNLOAD_SUCCESS);
  RunLoop();
  // Ensure consumer callback is not called.
  EXPECT_TRUE(download_results().empty());
  // Make AttachmentStore acknowledge writing attachment 1.
  store()->RespondToWrite(AttachmentStore::SUCCESS);
  RunLoop();
  // Ensure consumer callback is not called.
  EXPECT_TRUE(download_results().empty());

  // Make downloader return attachment 2.
  downloader()->RespondToDownload(attachment_ids[2],
                                  AttachmentDownloader::DOWNLOAD_SUCCESS);
  RunLoop();
  // Ensure consumer callback is not called.
  EXPECT_TRUE(download_results().empty());
  // Make AttachmentStore fail writing attachment 2.
  store()->RespondToWrite(AttachmentStore::UNSPECIFIED_ERROR);
  RunLoop();
  // Ensure consumer callback is not called.
  EXPECT_TRUE(download_results().empty());

  // Make downloader fail attachment 3.
  downloader()->RespondToDownload(
      attachment_ids[3], AttachmentDownloader::DOWNLOAD_UNSPECIFIED_ERROR);
  RunLoop();

  // Ensure callback is called
  EXPECT_FALSE(download_results().empty());
  // There should be only two attachments returned, 0 and 1.
  EXPECT_EQ(2U, last_download_attachments().size());
  EXPECT_TRUE(last_download_attachments().find(attachment_ids[0]) !=
              last_download_attachments().end());
  EXPECT_TRUE(last_download_attachments().find(attachment_ids[1]) !=
              last_download_attachments().end());
  EXPECT_TRUE(last_download_attachments().find(attachment_ids[2]) ==
              last_download_attachments().end());
  EXPECT_TRUE(last_download_attachments().find(attachment_ids[3]) ==
              last_download_attachments().end());
}

TEST_F(AttachmentServiceImplTest, GetOrDownload_NoDownloader) {
  // No downloader.
  InitializeAttachmentService(
      base::WrapUnique<MockAttachmentUploader>(new MockAttachmentUploader()),
      base::WrapUnique<MockAttachmentDownloader>(nullptr), this);

  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create(0, 0));
  attachment_service()->GetOrDownloadAttachments(attachment_ids,
                                                 download_callback());
  RunLoop();
  EXPECT_FALSE(store()->read_ids.empty());

  AttachmentIdSet local_attachments;
  store()->RespondToRead(local_attachments);
  RunLoop();
  ASSERT_EQ(1U, download_results().size());
  EXPECT_EQ(AttachmentService::GET_UNSPECIFIED_ERROR, download_results()[0]);
  EXPECT_TRUE(last_download_attachments().empty());
}

TEST_F(AttachmentServiceImplTest, UploadAttachments_Success) {
  AttachmentIdList attachment_ids;
  const unsigned num_attachments = 3;
  for (unsigned i = 0; i < num_attachments; ++i) {
    attachment_ids.push_back(AttachmentId::Create(0, 0));
  }
  attachment_service()->UploadAttachments(attachment_ids);
  RunLoop();
  ASSERT_EQ(1U, store()->set_reference_ids.size());
  EXPECT_EQ(AttachmentStore::SYNC, store()->set_reference_ids[0].first);
  for (unsigned i = 0; i < num_attachments; ++i) {
    RunLoopAndFireTimer();
    // See that the service has issued a read for at least one of the
    // attachments.
    ASSERT_GE(store()->read_ids.size(), 1U);
    store()->RespondToRead(AttachmentIdSetFromList(attachment_ids));
    RunLoop();
    ASSERT_GE(uploader()->upload_requests.size(), 1U);
    uploader()->RespondToUpload(uploader()->upload_requests.begin()->first,
                                AttachmentUploader::UPLOAD_SUCCESS);
  }
  RunLoop();
  ASSERT_EQ(0U, store()->read_ids.size());
  ASSERT_EQ(0U, uploader()->upload_requests.size());

  // See that all the attachments were uploaded.
  ASSERT_EQ(attachment_ids.size(), on_attachment_uploaded_list().size());
  for (auto iter = attachment_ids.begin(); iter != attachment_ids.end();
       ++iter) {
    EXPECT_THAT(on_attachment_uploaded_list(), testing::Contains(*iter));
  }
  EXPECT_EQ(num_attachments, store()->drop_ids.size());
}

TEST_F(AttachmentServiceImplTest, UploadAttachments_Success_NoDelegate) {
  InitializeAttachmentService(base::MakeUnique<MockAttachmentUploader>(),
                              base::MakeUnique<MockAttachmentDownloader>(),
                              nullptr);  // No delegate.

  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create(0, 0));
  attachment_service()->UploadAttachments(attachment_ids);
  RunLoopAndFireTimer();
  ASSERT_EQ(1U, store()->read_ids.size());
  ASSERT_EQ(0U, uploader()->upload_requests.size());
  store()->RespondToRead(AttachmentIdSetFromList(attachment_ids));
  RunLoop();
  ASSERT_EQ(0U, store()->read_ids.size());
  ASSERT_EQ(1U, uploader()->upload_requests.size());
  uploader()->RespondToUpload(*attachment_ids.begin(),
                              AttachmentUploader::UPLOAD_SUCCESS);
  RunLoop();
  ASSERT_TRUE(on_attachment_uploaded_list().empty());
}

TEST_F(AttachmentServiceImplTest, UploadAttachments_SomeMissingFromStore) {
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create(0, 0));
  attachment_ids.push_back(AttachmentId::Create(0, 0));
  attachment_service()->UploadAttachments(attachment_ids);
  RunLoopAndFireTimer();
  ASSERT_GE(store()->read_ids.size(), 1U);

  ASSERT_EQ(0U, uploader()->upload_requests.size());
  store()->RespondToRead(AttachmentIdSetFromList(attachment_ids));
  RunLoop();
  ASSERT_EQ(1U, uploader()->upload_requests.size());

  uploader()->RespondToUpload(uploader()->upload_requests.begin()->first,
                              AttachmentUploader::UPLOAD_SUCCESS);
  RunLoopAndFireTimer();
  ASSERT_EQ(1U, on_attachment_uploaded_list().size());
  ASSERT_GE(store()->read_ids.size(), 1U);
  // Not found!
  store()->RespondToRead(AttachmentIdSet());
  RunLoop();
  // No upload requests since the read failed.
  ASSERT_EQ(0U, uploader()->upload_requests.size());
  EXPECT_EQ(attachment_ids.size(), store()->drop_ids.size());
}

TEST_F(AttachmentServiceImplTest, UploadAttachments_AllMissingFromStore) {
  AttachmentIdList attachment_ids;
  const unsigned num_attachments = 2;
  for (unsigned i = 0; i < num_attachments; ++i) {
    attachment_ids.push_back(AttachmentId::Create(0, 0));
  }
  attachment_service()->UploadAttachments(attachment_ids);

  for (unsigned i = 0; i < num_attachments; ++i) {
    RunLoopAndFireTimer();
    ASSERT_GE(store()->read_ids.size(), 1U);
    // None found!
    store()->RespondToRead(AttachmentIdSet());
  }
  RunLoop();

  // Nothing uploaded.
  EXPECT_EQ(0U, uploader()->upload_requests.size());
  // See that the delegate was never called.
  ASSERT_EQ(0U, on_attachment_uploaded_list().size());
  EXPECT_EQ(num_attachments, store()->drop_ids.size());
}

TEST_F(AttachmentServiceImplTest, UploadAttachments_NoUploader) {
  InitializeAttachmentService(base::WrapUnique<MockAttachmentUploader>(nullptr),
                              base::MakeUnique<MockAttachmentDownloader>(),
                              this);

  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create(0, 0));
  attachment_service()->UploadAttachments(attachment_ids);
  RunLoop();
  EXPECT_EQ(0U, store()->read_ids.size());
  ASSERT_EQ(0U, on_attachment_uploaded_list().size());
  EXPECT_EQ(0U, store()->drop_ids.size());
}

// Upload three attachments.  For one of them, server responds with error.
TEST_F(AttachmentServiceImplTest, UploadAttachments_OneUploadFails) {
  AttachmentIdList attachment_ids;
  const unsigned num_attachments = 3;
  for (unsigned i = 0; i < num_attachments; ++i) {
    attachment_ids.push_back(AttachmentId::Create(0, 0));
  }
  attachment_service()->UploadAttachments(attachment_ids);

  for (unsigned i = 0; i < 3; ++i) {
    RunLoopAndFireTimer();
    ASSERT_GE(store()->read_ids.size(), 1U);
    store()->RespondToRead(AttachmentIdSetFromList(attachment_ids));
    RunLoop();
    ASSERT_EQ(1U, uploader()->upload_requests.size());
    AttachmentUploader::UploadResult result =
        AttachmentUploader::UPLOAD_SUCCESS;
    // Fail the 2nd one.
    if (i == 2U) {
      result = AttachmentUploader::UPLOAD_UNSPECIFIED_ERROR;
    } else {
      result = AttachmentUploader::UPLOAD_SUCCESS;
    }
    uploader()->RespondToUpload(uploader()->upload_requests.begin()->first,
                                result);
    RunLoop();
  }
  ASSERT_EQ(2U, on_attachment_uploaded_list().size());
  EXPECT_EQ(num_attachments, store()->drop_ids.size());
}

// Attempt an upload, respond with transient error to trigger backoff, issue
// network disconnect/connect events and see that backoff is cleared.
TEST_F(AttachmentServiceImplTest,
       UploadAttachments_ResetBackoffAfterNetworkChange) {
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create(0, 0));
  attachment_service()->UploadAttachments(attachment_ids);

  RunLoopAndFireTimer();
  ASSERT_EQ(1U, store()->read_ids.size());
  store()->RespondToRead(AttachmentIdSetFromList(attachment_ids));
  RunLoop();
  ASSERT_EQ(1U, uploader()->upload_requests.size());

  uploader()->RespondToUpload(uploader()->upload_requests.begin()->first,
                              AttachmentUploader::UPLOAD_TRANSIENT_ERROR);
  RunLoop();

  // See that we are in backoff.
  ASSERT_TRUE(mock_timer()->IsRunning());
  ASSERT_GT(mock_timer()->GetCurrentDelay(), base::TimeDelta());

  // Issue a network disconnect event.
  network_change_notifier()->NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  RunLoop();

  // Still in backoff.
  ASSERT_TRUE(mock_timer()->IsRunning());
  ASSERT_GT(mock_timer()->GetCurrentDelay(), base::TimeDelta());

  // Issue a network connect event.
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  RunLoop();

  // No longer in backoff.
  ASSERT_TRUE(mock_timer()->IsRunning());
  ASSERT_EQ(base::TimeDelta(), mock_timer()->GetCurrentDelay());
}

}  // namespace syncer
