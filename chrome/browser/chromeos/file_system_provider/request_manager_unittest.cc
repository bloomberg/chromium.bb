// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Logs calls of the success and error callbacks on requests.
class EventLogger {
 public:
  class SuccessEvent {
   public:
    SuccessEvent(scoped_ptr<base::DictionaryValue> result, bool has_next)
        : result_(result.Pass()), has_next_(has_next) {}
    ~SuccessEvent() {}

    base::DictionaryValue* result() { return result_.get(); }
    bool has_next() { return has_next_; }

   private:
    scoped_ptr<base::DictionaryValue> result_;
    bool has_next_;
  };

  class ErrorEvent {
   public:
    explicit ErrorEvent(base::File::Error error) : error_(error) {}
    ~ErrorEvent() {}

    base::File::Error error() { return error_; }

   private:
    base::File::Error error_;
  };

  EventLogger() : weak_ptr_factory_(this) {}
  virtual ~EventLogger() {}

  void OnSuccess(scoped_ptr<base::DictionaryValue> result, bool has_next) {
    success_events_.push_back(new SuccessEvent(result.Pass(), has_next));
  }

  void OnError(base::File::Error error) {
    error_events_.push_back(new ErrorEvent(error));
  }

  ScopedVector<SuccessEvent>& success_events() { return success_events_; }
  ScopedVector<ErrorEvent>& error_events() { return error_events_; }

  base::WeakPtr<EventLogger> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  ScopedVector<SuccessEvent> success_events_;
  ScopedVector<ErrorEvent> error_events_;
  base::WeakPtrFactory<EventLogger> weak_ptr_factory_;
};

}  // namespace

class FileSystemProviderRequestManagerTest : public testing::Test {
 protected:
  FileSystemProviderRequestManagerTest() {}
  virtual ~FileSystemProviderRequestManagerTest() {}

  virtual void SetUp() OVERRIDE {
    request_manager_.reset(new RequestManager());
  }

  scoped_ptr<RequestManager> request_manager_;
};

TEST_F(FileSystemProviderRequestManagerTest, CreateAndFulFill) {
  EventLogger logger;

  int request_id = request_manager_->CreateRequest(
      base::Bind(&EventLogger::OnSuccess, logger.GetWeakPtr()),
      base::Bind(&EventLogger::OnError, logger.GetWeakPtr()));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue());
  const bool has_next = false;
  response->SetString("path", "i-like-vanilla");

  bool result =
      request_manager_->FulfillRequest(request_id, response.Pass(), has_next);
  EXPECT_TRUE(result);

  // Validate if the callback has correct arguments.
  ASSERT_EQ(1u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());
  EventLogger::SuccessEvent* event = logger.success_events()[0];
  ASSERT_TRUE(event->result());
  std::string response_test_string;
  EXPECT_TRUE(event->result()->GetString("path", &response_test_string));
  EXPECT_EQ("i-like-vanilla", response_test_string);
  EXPECT_FALSE(event->has_next());

  // Confirm, that the request is removed. Basically, fulfilling again for the
  // same request, should fail.
  {
    scoped_ptr<base::DictionaryValue> response;
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

  int request_id = request_manager_->CreateRequest(
      base::Bind(&EventLogger::OnSuccess, logger.GetWeakPtr()),
      base::Bind(&EventLogger::OnError, logger.GetWeakPtr()));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  scoped_ptr<base::DictionaryValue> response;
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

  int request_id = request_manager_->CreateRequest(
      base::Bind(&EventLogger::OnSuccess, logger.GetWeakPtr()),
      base::Bind(&EventLogger::OnError, logger.GetWeakPtr()));

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
    scoped_ptr<base::DictionaryValue> response;
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

  int request_id = request_manager_->CreateRequest(
      base::Bind(&EventLogger::OnSuccess, logger.GetWeakPtr()),
      base::Bind(&EventLogger::OnError, logger.GetWeakPtr()));

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

  int request_id = request_manager_->CreateRequest(
      base::Bind(&EventLogger::OnSuccess, logger.GetWeakPtr()),
      base::Bind(&EventLogger::OnError, logger.GetWeakPtr()));

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

  int first_request_id = request_manager_->CreateRequest(
      base::Bind(&EventLogger::OnSuccess, logger.GetWeakPtr()),
      base::Bind(&EventLogger::OnError, logger.GetWeakPtr()));

  int second_request_id = request_manager_->CreateRequest(
      base::Bind(&EventLogger::OnSuccess, logger.GetWeakPtr()),
      base::Bind(&EventLogger::OnError, logger.GetWeakPtr()));

  EXPECT_EQ(1, first_request_id);
  EXPECT_EQ(2, second_request_id);
}

TEST_F(FileSystemProviderRequestManagerTest, AbortOnDestroy) {
  EventLogger logger;

  {
    RequestManager request_manager;
    int request_id = request_manager.CreateRequest(
        base::Bind(&EventLogger::OnSuccess, logger.GetWeakPtr()),
        base::Bind(&EventLogger::OnError, logger.GetWeakPtr()));

    EXPECT_EQ(1, request_id);
    EXPECT_EQ(0u, logger.success_events().size());
    EXPECT_EQ(0u, logger.error_events().size());
  }

  // All active requests should be aborted in the destructor of RequestManager.
  EventLogger::ErrorEvent* event = logger.error_events()[0];
  ASSERT_EQ(1u, logger.error_events().size());
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, event->error());

  EXPECT_EQ(0u, logger.success_events().size());
}

}  // namespace file_system_provider
}  // namespace chromeos
