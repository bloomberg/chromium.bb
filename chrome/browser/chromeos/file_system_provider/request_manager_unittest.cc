// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Logs calls of the success and error callbacks on requests.
class EventLogger {
 public:
  class ExecuteEvent {
   public:
    explicit ExecuteEvent(int request_id) : request_id_(request_id) {}
    virtual ~ExecuteEvent() {}

    int request_id() { return request_id_; }

   private:
    int request_id_;
  };

  class SuccessEvent {
   public:
    SuccessEvent(int request_id, scoped_ptr<RequestValue> result, bool has_next)
        : request_id_(request_id),
          result_(result.Pass()),
          has_next_(has_next) {}
    virtual ~SuccessEvent() {}

    int request_id() { return request_id_; }
    RequestValue* result() { return result_.get(); }
    bool has_next() { return has_next_; }

   private:
    int request_id_;
    scoped_ptr<RequestValue> result_;
    bool has_next_;
  };

  class ErrorEvent {
   public:
    ErrorEvent(int request_id, base::File::Error error)
        : request_id_(request_id), error_(error) {}
    virtual ~ErrorEvent() {}

    int request_id() { return request_id_; }
    base::File::Error error() { return error_; }

   private:
    int request_id_;
    base::File::Error error_;
  };

  EventLogger() : weak_ptr_factory_(this) {}
  virtual ~EventLogger() {}

  void OnExecute(int request_id) {
    execute_events_.push_back(new ExecuteEvent(request_id));
  }

  void OnSuccess(int request_id,
                 scoped_ptr<RequestValue> result,
                 bool has_next) {
    success_events_.push_back(
        new SuccessEvent(request_id, result.Pass(), has_next));
  }

  void OnError(int request_id, base::File::Error error) {
    error_events_.push_back(new ErrorEvent(request_id, error));
  }

  ScopedVector<ExecuteEvent>& execute_events() { return execute_events_; }
  ScopedVector<SuccessEvent>& success_events() { return success_events_; }
  ScopedVector<ErrorEvent>& error_events() { return error_events_; }

  base::WeakPtr<EventLogger> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  ScopedVector<ExecuteEvent> execute_events_;
  ScopedVector<SuccessEvent> success_events_;
  ScopedVector<ErrorEvent> error_events_;
  base::WeakPtrFactory<EventLogger> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EventLogger);
};

// Fake handler, which forwards callbacks to the logger. The handler is owned
// by a request manager, however the logger is owned by tests.
class FakeHandler : public RequestManager::HandlerInterface {
 public:
  // The handler can outlive the passed logger, so using a weak pointer. The
  // |execute_reply| value will be returned for the Execute() call.
  FakeHandler(base::WeakPtr<EventLogger> logger, bool execute_reply)
      : logger_(logger), execute_reply_(execute_reply) {}

  // RequestManager::Handler overrides.
  virtual bool Execute(int request_id) OVERRIDE {
    if (logger_.get())
      logger_->OnExecute(request_id);

    return execute_reply_;
  }

  // RequestManager::Handler overrides.
  virtual void OnSuccess(int request_id,
                         scoped_ptr<RequestValue> result,
                         bool has_next) OVERRIDE {
    if (logger_.get())
      logger_->OnSuccess(request_id, result.Pass(), has_next);
  }

  // RequestManager::Handler overrides.
  virtual void OnError(int request_id, base::File::Error error) OVERRIDE {
    if (logger_.get())
      logger_->OnError(request_id, error);
  }

  virtual ~FakeHandler() {}

 private:
  base::WeakPtr<EventLogger> logger_;
  bool execute_reply_;
  DISALLOW_COPY_AND_ASSIGN(FakeHandler);
};

}  // namespace

class FileSystemProviderRequestManagerTest : public testing::Test {
 protected:
  FileSystemProviderRequestManagerTest() {}
  virtual ~FileSystemProviderRequestManagerTest() {}

  virtual void SetUp() OVERRIDE {
    request_manager_.reset(new RequestManager());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<RequestManager> request_manager_;
};

TEST_F(FileSystemProviderRequestManagerTest, CreateAndFulFill) {
  EventLogger logger;

  const int request_id = request_manager_->CreateRequest(
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), true /* execute_reply */)));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  scoped_ptr<RequestValue> response(
      RequestValue::CreateForTesting("i-like-vanilla"));
  const bool has_next = false;

  bool result =
      request_manager_->FulfillRequest(request_id, response.Pass(), has_next);
  EXPECT_TRUE(result);

  // Validate if the callback has correct arguments.
  ASSERT_EQ(1u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());
  EventLogger::SuccessEvent* event = logger.success_events()[0];
  ASSERT_TRUE(event->result());
  const std::string* response_test_string = event->result()->testing_params();
  ASSERT_TRUE(response_test_string);
  EXPECT_EQ("i-like-vanilla", *response_test_string);
  EXPECT_FALSE(event->has_next());

  // Confirm, that the request is removed. Basically, fulfilling again for the
  // same request, should fail.
  {
    scoped_ptr<RequestValue> response;
    bool retry =
        request_manager_->FulfillRequest(request_id, response.Pass(), has_next);
    EXPECT_FALSE(retry);
  }

  // Rejecting should also fail.
  {
    bool retry = request_manager_->RejectRequest(request_id,
                                                 base::File::FILE_ERROR_FAILED);
    EXPECT_FALSE(retry);
  }
}

TEST_F(FileSystemProviderRequestManagerTest, CreateAndFulFill_WithHasNext) {
  EventLogger logger;

  const int request_id = request_manager_->CreateRequest(
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), true /* execute_reply */)));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  scoped_ptr<RequestValue> response;
  const bool has_next = true;

  bool result =
      request_manager_->FulfillRequest(request_id, response.Pass(), has_next);
  EXPECT_TRUE(result);

  // Validate if the callback has correct arguments.
  ASSERT_EQ(1u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());
  EventLogger::SuccessEvent* event = logger.success_events()[0];
  EXPECT_FALSE(event->result());
  EXPECT_TRUE(event->has_next());

  // Confirm, that the request is not removed (since it has has_next == true).
  // Basically, fulfilling again for the same request, should not fail.
  {
    bool new_has_next = false;
    bool retry = request_manager_->FulfillRequest(
        request_id, response.Pass(), new_has_next);
    EXPECT_TRUE(retry);
  }

  // Since |new_has_next| is false, then the request should be removed. To check
  // it, try to fulfill again, what should fail.
  {
    bool new_has_next = false;
    bool retry = request_manager_->FulfillRequest(
        request_id, response.Pass(), new_has_next);
    EXPECT_FALSE(retry);
  }
}

TEST_F(FileSystemProviderRequestManagerTest, CreateAndReject) {
  EventLogger logger;

  const int request_id = request_manager_->CreateRequest(
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), true /* execute_reply */)));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  base::File::Error error = base::File::FILE_ERROR_NO_MEMORY;
  bool result = request_manager_->RejectRequest(request_id, error);
  EXPECT_TRUE(result);

  // Validate if the callback has correct arguments.
  ASSERT_EQ(1u, logger.error_events().size());
  EXPECT_EQ(0u, logger.success_events().size());
  EventLogger::ErrorEvent* event = logger.error_events()[0];
  EXPECT_EQ(error, event->error());

  // Confirm, that the request is removed. Basically, fulfilling again for the
  // same request, should fail.
  {
    scoped_ptr<RequestValue> response;
    bool has_next = false;
    bool retry =
        request_manager_->FulfillRequest(request_id, response.Pass(), has_next);
    EXPECT_FALSE(retry);
  }

  // Rejecting should also fail.
  {
    bool retry = request_manager_->RejectRequest(request_id, error);
    EXPECT_FALSE(retry);
  }
}

TEST_F(FileSystemProviderRequestManagerTest,
       CreateAndFulfillWithWrongRequestId) {
  EventLogger logger;

  const int request_id = request_manager_->CreateRequest(
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), true /* execute_reply */)));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  base::File::Error error = base::File::FILE_ERROR_NO_MEMORY;
  bool result = request_manager_->RejectRequest(request_id + 1, error);
  EXPECT_FALSE(result);

  // Callbacks should not be called.
  EXPECT_EQ(0u, logger.error_events().size());
  EXPECT_EQ(0u, logger.success_events().size());

  // Confirm, that the request hasn't been removed, by rejecting it correctly.
  {
    bool retry = request_manager_->RejectRequest(request_id, error);
    EXPECT_TRUE(retry);
  }
}

TEST_F(FileSystemProviderRequestManagerTest,
       CreateAndRejectWithWrongRequestId) {
  EventLogger logger;

  const int request_id = request_manager_->CreateRequest(
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), true /* execute_reply */)));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  base::File::Error error = base::File::FILE_ERROR_NO_MEMORY;
  bool result = request_manager_->RejectRequest(request_id + 1, error);
  EXPECT_FALSE(result);

  // Callbacks should not be called.
  EXPECT_EQ(0u, logger.error_events().size());
  EXPECT_EQ(0u, logger.success_events().size());

  // Confirm, that the request hasn't been removed, by rejecting it correctly.
  {
    bool retry = request_manager_->RejectRequest(request_id, error);
    EXPECT_TRUE(retry);
  }
}

TEST_F(FileSystemProviderRequestManagerTest, UniqueIds) {
  EventLogger logger;

  const int first_request_id = request_manager_->CreateRequest(
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), true /* execute_reply */)));

  const int second_request_id = request_manager_->CreateRequest(
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), true /* execute_reply */)));

  EXPECT_EQ(1, first_request_id);
  EXPECT_EQ(2, second_request_id);
}

TEST_F(FileSystemProviderRequestManagerTest, AbortOnDestroy) {
  EventLogger logger;

  {
    RequestManager request_manager;
    const int request_id = request_manager.CreateRequest(
        make_scoped_ptr<RequestManager::HandlerInterface>(
            new FakeHandler(logger.GetWeakPtr(), true /* execute_reply */)));

    EXPECT_EQ(1, request_id);
    EXPECT_EQ(0u, logger.success_events().size());
    EXPECT_EQ(0u, logger.error_events().size());
  }

  // All active requests should be aborted in the destructor of RequestManager.
  ASSERT_EQ(1u, logger.error_events().size());
  EventLogger::ErrorEvent* event = logger.error_events()[0];
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, event->error());

  EXPECT_EQ(0u, logger.success_events().size());
}

TEST_F(FileSystemProviderRequestManagerTest, AbortOnTimeout) {
  EventLogger logger;
  base::RunLoop run_loop;

  request_manager_->SetTimeoutForTests(base::TimeDelta::FromSeconds(0));
  const int request_id = request_manager_->CreateRequest(
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), true /* execute_reply */)));
  EXPECT_LT(0, request_id);

  // Wait until the request is timeouted.
  run_loop.RunUntilIdle();

  ASSERT_EQ(1u, logger.error_events().size());
  EventLogger::ErrorEvent* event = logger.error_events()[0];
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, event->error());
}

}  // namespace file_system_provider
}  // namespace chromeos
