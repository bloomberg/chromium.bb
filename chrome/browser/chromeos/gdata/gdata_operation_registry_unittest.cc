// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_operation_registry.h"

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace gdata {

namespace {

class MockOperation : public GDataOperationRegistry::Operation,
                      public base::SupportsWeakPtr<MockOperation> {
 public:
  MockOperation(GDataOperationRegistry* registry,
                GDataOperationRegistry::OperationType type,
                const FilePath& path)
      : GDataOperationRegistry::Operation(registry, type, path) {}

  MOCK_METHOD0(DoCancel, void());

  // Make them public so that they can be called from test cases.
  using GDataOperationRegistry::Operation::NotifyStart;
  using GDataOperationRegistry::Operation::NotifyProgress;
  using GDataOperationRegistry::Operation::NotifyFinish;
};

class MockUploadOperation : public MockOperation {
 public:
  explicit MockUploadOperation(GDataOperationRegistry* registry)
      : MockOperation(registry,
                      GDataOperationRegistry::OPERATION_UPLOAD,
                      FilePath("/dummy/upload")) {}
};

class MockDownloadOperation : public MockOperation {
 public:
  explicit MockDownloadOperation(GDataOperationRegistry* registry)
      : MockOperation(registry,
                      GDataOperationRegistry::OPERATION_DOWNLOAD,
                      FilePath("/dummy/download")) {}
};

class MockOtherOperation : public MockOperation {
 public:
  explicit MockOtherOperation(GDataOperationRegistry* registry)
      : MockOperation(registry,
                      GDataOperationRegistry::OPERATION_OTHER,
                      FilePath("/dummy/other")) {}
};

class TestObserver : public GDataOperationRegistry::Observer {
 public:
  virtual void OnProgressUpdate(
      const std::vector<GDataOperationRegistry::ProgressStatus>& list)
      OVERRIDE {
    status_ = list;
  }

  const std::vector<GDataOperationRegistry::ProgressStatus>& status() const {
    return status_;
  }

 private:
  std::vector<GDataOperationRegistry::ProgressStatus> status_;
};

class ProgressMatcher
    : public ::testing::MatcherInterface<
        const GDataOperationRegistry::ProgressStatus&> {
 public:
  ProgressMatcher(int64 expected_current, int64 expected_total)
      : expected_current_(expected_current),
        expected_total_(expected_total) {}

  virtual bool MatchAndExplain(
      const GDataOperationRegistry::ProgressStatus& status,
      testing::MatchResultListener* /* listener */) const OVERRIDE {
    return status.progress_current == expected_current_ &&
        status.progress_total == expected_total_;
  }

  virtual void DescribeTo(::std::ostream* os) const OVERRIDE {
    *os << "current / total equals " << expected_current_ << " / " <<
        expected_total_;
  }

  virtual void DescribeNegationTo(::std::ostream* os) const OVERRIDE {
    *os << "current / total does not equal " << expected_current_ << " / " <<
        expected_total_;
  }

 private:
  const int64 expected_current_;
  const int64 expected_total_;
};

testing::Matcher<const GDataOperationRegistry::ProgressStatus&> Progress(
    int64 current, int64 total) {
  return testing::MakeMatcher(new ProgressMatcher(current, total));
}

}  // namespace

// Pretty-prints ProgressStatus for testing purpose.
std::ostream& operator<<(std::ostream& os,
                         const GDataOperationRegistry::ProgressStatus& status) {
  return os << status.DebugString();
}

class GDataOperationRegistryTest : public testing::Test {
 protected:
  GDataOperationRegistryTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(GDataOperationRegistryTest, OneSuccess) {
  TestObserver observer;
  GDataOperationRegistry registry;
  registry.AddObserver(&observer);

  base::WeakPtr<MockOperation> op1 =
      (new MockUploadOperation(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel()).Times(0);

  EXPECT_EQ(0U, observer.status().size());
  op1->NotifyStart();
  EXPECT_THAT(observer.status(), ElementsAre(Progress(0, -1)));
  op1->NotifyProgress(0, 100);
  EXPECT_THAT(observer.status(), ElementsAre(Progress(0, 100)));
  op1->NotifyProgress(100, 100);
  EXPECT_THAT(observer.status(), ElementsAre(Progress(100, 100)));
  op1->NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
  // Contains one "COMPLETED" notification.
  EXPECT_THAT(observer.status(), ElementsAre(Progress(100, 100)));
  // Then it is removed.
  EXPECT_EQ(0U, registry.GetProgressStatusList().size());
  EXPECT_EQ(NULL, op1.get()); // deleted
}

TEST_F(GDataOperationRegistryTest, OneCancel) {
  TestObserver observer;
  GDataOperationRegistry registry;
  registry.AddObserver(&observer);

  base::WeakPtr<MockOperation> op1 =
      (new MockUploadOperation(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel());

  EXPECT_EQ(0U, observer.status().size());
  op1->NotifyStart();
  EXPECT_THAT(observer.status(), ElementsAre(Progress(0, -1)));
  op1->NotifyProgress(0, 100);
  EXPECT_THAT(observer.status(), ElementsAre(Progress(0, 100)));
  registry.CancelAll();
  EXPECT_THAT(observer.status(), ElementsAre(Progress(0, 100)));
  EXPECT_EQ(0U, registry.GetProgressStatusList().size());
  EXPECT_EQ(NULL, op1.get()); // deleted
}

TEST_F(GDataOperationRegistryTest, TwoSuccess) {
  TestObserver observer;
  GDataOperationRegistry registry;
  registry.AddObserver(&observer);

  base::WeakPtr<MockOperation> op1 =
      (new MockUploadOperation(&registry))->AsWeakPtr();
  base::WeakPtr<MockOperation> op2 =
      (new MockDownloadOperation(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel()).Times(0);
  EXPECT_CALL(*op2, DoCancel()).Times(0);

  EXPECT_EQ(0U, observer.status().size());
  op1->NotifyStart();
  op1->NotifyProgress(0, 100);
  EXPECT_THAT(observer.status(), ElementsAre(Progress(0, 100)));
  op2->NotifyStart();
  op2->NotifyProgress(0, 200);
  op1->NotifyProgress(50, 100);
  EXPECT_THAT(observer.status(), ElementsAre(Progress(50, 100),
                                             Progress(0, 200)));
  op1->NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
  EXPECT_THAT(observer.status(), ElementsAre(Progress(50, 100),
                                             Progress(0, 200)));
  EXPECT_EQ(1U, registry.GetProgressStatusList().size());
  op2->NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
  EXPECT_THAT(observer.status(), ElementsAre(Progress(0, 200)));
  EXPECT_EQ(0U, registry.GetProgressStatusList().size());
  EXPECT_EQ(NULL, op1.get()); // deleted
  EXPECT_EQ(NULL, op2.get()); // deleted
}

TEST_F(GDataOperationRegistryTest, ThreeCancel) {
  TestObserver observer;
  GDataOperationRegistry registry;
  registry.AddObserver(&observer);

  base::WeakPtr<MockOperation> op1 =
      (new MockUploadOperation(&registry))->AsWeakPtr();
  base::WeakPtr<MockOperation> op2 =
      (new MockDownloadOperation(&registry))->AsWeakPtr();
  base::WeakPtr<MockOperation> op3 =
      (new MockOtherOperation(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel());
  EXPECT_CALL(*op2, DoCancel());
  EXPECT_CALL(*op3, DoCancel());

  EXPECT_EQ(0U, observer.status().size());
  op1->NotifyStart();
  EXPECT_EQ(1U, observer.status().size());
  op2->NotifyStart();
  EXPECT_EQ(2U, observer.status().size());
  op3->NotifyStart();
  EXPECT_EQ(2U, observer.status().size()); // only upload/download is reported.
  registry.CancelAll();
  EXPECT_EQ(1U, observer.status().size()); // holds the last one "COMPLETED"
  EXPECT_EQ(0U, registry.GetProgressStatusList().size());
  EXPECT_EQ(NULL, op1.get()); // deleted
  EXPECT_EQ(NULL, op2.get()); // deleted
  EXPECT_EQ(NULL, op3.get()); // deleted. CancelAll cares all operations.
}

TEST_F(GDataOperationRegistryTest, RestartOperation) {
  TestObserver observer;
  GDataOperationRegistry registry;
  registry.AddObserver(&observer);

  base::WeakPtr<MockOperation> op1 =
      (new MockUploadOperation(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel()).Times(0);

  op1->NotifyStart();
  EXPECT_EQ(1U, registry.GetProgressStatusList().size());
  op1->NotifyStart(); // restart
  EXPECT_EQ(1U, registry.GetProgressStatusList().size());
  op1->NotifyProgress(0, 200);
  op1->NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
  EXPECT_EQ(0U, registry.GetProgressStatusList().size());
  EXPECT_EQ(NULL, op1.get()); // deleted
}

}  // namespace gdata
