// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/task_tracker.h"

#include "base/run_loop.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/fake_distiller.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::_;

namespace dom_distiller {
namespace test {

class FakeViewRequestDelegate : public ViewRequestDelegate {
 public:
  virtual ~FakeViewRequestDelegate() {}
  MOCK_METHOD1(OnArticleReady, void(DistilledPageProto* proto));
};

class TestCancelCallback {
 public:
  TestCancelCallback() : cancelled_(false) {}
  TaskTracker::CancelCallback GetCallback() {
    return base::Bind(&TestCancelCallback::Cancel, base::Unretained(this));
  }
  void Cancel(TaskTracker*) { cancelled_ = true; }
  bool Cancelled() { return cancelled_; }

 private:
  bool cancelled_;
};

class MockSaveCallback {
 public:
  MOCK_METHOD2(Save, void(const ArticleEntry&, DistilledPageProto*));
};

class DomDistillerTaskTrackerTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    message_loop_.reset(new base::MessageLoop());
    entry_id_ = "id0";
    page_0_url_ = GURL("http://www.example.com/1");
    page_1_url_ = GURL("http://www.example.com/2");
  }

  ArticleEntry GetDefaultEntry() {
    ArticleEntry entry;
    entry.set_entry_id(entry_id_);
    ArticleEntryPage* page0 = entry.add_pages();
    ArticleEntryPage* page1 = entry.add_pages();
    page0->set_url(page_0_url_.spec());
    page1->set_url(page_1_url_.spec());
    return entry;
  }

 protected:
  scoped_ptr<base::MessageLoop> message_loop_;
  std::string entry_id_;
  GURL page_0_url_;
  GURL page_1_url_;
};

TEST_F(DomDistillerTaskTrackerTest, TestHasEntryId) {
  MockDistillerFactory distiller_factory;
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback());
  EXPECT_TRUE(task_tracker.HasEntryId(entry_id_));
  EXPECT_FALSE(task_tracker.HasEntryId("other_id"));
}

TEST_F(DomDistillerTaskTrackerTest, TestHasUrl) {
  MockDistillerFactory distiller_factory;
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback());
  EXPECT_TRUE(task_tracker.HasUrl(page_0_url_));
  EXPECT_TRUE(task_tracker.HasUrl(page_1_url_));
  EXPECT_FALSE(task_tracker.HasUrl(GURL("http://other.url/")));
}

TEST_F(DomDistillerTaskTrackerTest, TestViewerCancelled) {
  MockDistillerFactory distiller_factory;
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback());

  FakeViewRequestDelegate viewer_delegate;
  FakeViewRequestDelegate viewer_delegate2;
  scoped_ptr<ViewerHandle> handle(task_tracker.AddViewer(&viewer_delegate));
  scoped_ptr<ViewerHandle> handle2(task_tracker.AddViewer(&viewer_delegate2));

  EXPECT_FALSE(cancel_callback.Cancelled());
  handle.reset();
  EXPECT_FALSE(cancel_callback.Cancelled());
  handle2.reset();
  EXPECT_TRUE(cancel_callback.Cancelled());
}

TEST_F(DomDistillerTaskTrackerTest, TestViewerCancelledWithSaveRequest) {
  MockDistillerFactory distiller_factory;
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback());

  FakeViewRequestDelegate viewer_delegate;
  scoped_ptr<ViewerHandle> handle(task_tracker.AddViewer(&viewer_delegate));
  EXPECT_FALSE(cancel_callback.Cancelled());

  MockSaveCallback save_callback;
  task_tracker.SetSaveCallback(
      base::Bind(&MockSaveCallback::Save, base::Unretained(&save_callback)));
  handle.reset();

  // Since there is a pending save request, the task shouldn't be cancelled.
  EXPECT_FALSE(cancel_callback.Cancelled());
}

TEST_F(DomDistillerTaskTrackerTest, TestViewerNotifiedOnDistillationComplete) {
  MockDistillerFactory distiller_factory;
  FakeDistiller* distiller = new FakeDistiller();
  EXPECT_CALL(distiller_factory, CreateDistillerImpl())
      .WillOnce(Return(distiller));
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback());

  FakeViewRequestDelegate viewer_delegate;
  scoped_ptr<ViewerHandle> handle(task_tracker.AddViewer(&viewer_delegate));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(viewer_delegate, OnArticleReady(_));

  task_tracker.StartDistiller(&distiller_factory);
  distiller->RunDistillerCallback(make_scoped_ptr(new DistilledPageProto));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cancel_callback.Cancelled());
}

TEST_F(DomDistillerTaskTrackerTest,
       TestSaveCallbackCalledOnDistillationComplete) {
  MockDistillerFactory distiller_factory;
  FakeDistiller* distiller = new FakeDistiller();
  EXPECT_CALL(distiller_factory, CreateDistillerImpl())
      .WillOnce(Return(distiller));
  TestCancelCallback cancel_callback;
  TaskTracker task_tracker(GetDefaultEntry(), cancel_callback.GetCallback());

  MockSaveCallback save_callback;
  task_tracker.SetSaveCallback(
      base::Bind(&MockSaveCallback::Save, base::Unretained(&save_callback)));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(save_callback, Save(_, _));

  task_tracker.StartDistiller(&distiller_factory);
  distiller->RunDistillerCallback(make_scoped_ptr(new DistilledPageProto));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(cancel_callback.Cancelled());
}

}  // namespace test
}  // namespace dom_distiller
