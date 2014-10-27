// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "extensions/browser/mojo/stash_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class StashServiceTest : public testing::Test, public mojo::ErrorHandler {
 public:
  enum Event {
    EVENT_NONE,
    EVENT_STASH_RETRIEVED,
  };

  StashServiceTest() {}

  void SetUp() override {
    expecting_error_ = false;
    expected_event_ = EVENT_NONE;
    stash_backend_.reset(new StashBackend);
    stash_backend_->BindToRequest(mojo::GetProxy(&stash_service_));
    stash_service_.set_error_handler(this);
  }

  void OnConnectionError() override { FAIL() << "Unexpected connection error"; }

  mojo::Array<StashedObjectPtr> RetrieveStash() {
    mojo::Array<StashedObjectPtr> stash;
    stash_service_->RetrieveStash(base::Bind(
        &StashServiceTest::StashRetrieved, base::Unretained(this), &stash));
    WaitForEvent(EVENT_STASH_RETRIEVED);
    return stash.Pass();
  }

  void StashRetrieved(mojo::Array<StashedObjectPtr>* output,
                      mojo::Array<StashedObjectPtr> stash) {
    *output = stash.Pass();
    EventReceived(EVENT_STASH_RETRIEVED);
  }

  void WaitForEvent(Event event) {
    expected_event_ = event;
    base::RunLoop run_loop;
    stop_run_loop_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void EventReceived(Event event) {
    if (event == expected_event_ && !stop_run_loop_.is_null())
      stop_run_loop_.Run();
  }

 protected:
  base::MessageLoop message_loop_;
  base::Closure stop_run_loop_;
  scoped_ptr<StashBackend> stash_backend_;
  Event expected_event_;
  bool expecting_error_;
  mojo::InterfacePtr<StashService> stash_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StashServiceTest);
};

// Test that adding stashed objects in multiple calls can all be retrieved by a
// Retrieve call.
TEST_F(StashServiceTest, AddTwiceAndRetrieve) {
  mojo::Array<StashedObjectPtr> stashed_objects;
  StashedObjectPtr stashed_object(StashedObject::New());
  stashed_object->id = "test type";
  stashed_object->data.push_back(1);
  stashed_object->stashed_handles = mojo::Array<mojo::ScopedHandle>(0);
  stashed_objects.push_back(stashed_object.Pass());
  stash_service_->AddToStash(stashed_objects.Pass());
  stashed_object = StashedObject::New();
  stashed_object->id = "test type2";
  stashed_object->data.push_back(2);
  stashed_object->data.push_back(3);
  stashed_object->stashed_handles = mojo::Array<mojo::ScopedHandle>(0);
  stashed_objects.push_back(stashed_object.Pass());
  stash_service_->AddToStash(stashed_objects.Pass());
  stashed_objects = RetrieveStash();
  ASSERT_EQ(2u, stashed_objects.size());
  EXPECT_EQ("test type", stashed_objects[0]->id);
  EXPECT_EQ(0u, stashed_objects[0]->stashed_handles.size());
  EXPECT_EQ(1u, stashed_objects[0]->data.size());
  EXPECT_EQ(1, stashed_objects[0]->data[0]);
  EXPECT_EQ("test type2", stashed_objects[1]->id);
  EXPECT_EQ(0u, stashed_objects[1]->stashed_handles.size());
  EXPECT_EQ(2u, stashed_objects[1]->data.size());
  EXPECT_EQ(2, stashed_objects[1]->data[0]);
  EXPECT_EQ(3, stashed_objects[1]->data[1]);
}

// Test that handles survive a round-trip through the stash.
TEST_F(StashServiceTest, StashAndRetrieveHandles) {
  mojo::Array<StashedObjectPtr> stashed_objects;
  StashedObjectPtr stashed_object(StashedObject::New());
  stashed_object->id = "test type";
  stashed_object->data.push_back(1);

  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::ScopedDataPipeProducerHandle producer;
  MojoCreateDataPipeOptions options = {
      sizeof(options), MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE, 1, 1,
  };
  mojo::CreateDataPipe(&options, &producer, &consumer);
  uint32_t num_bytes = 1;
  MojoResult result = mojo::WriteDataRaw(
      producer.get(), "1", &num_bytes, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(1u, num_bytes);

  stashed_object->stashed_handles.push_back(
      mojo::ScopedHandle::From(producer.Pass()));
  stashed_object->stashed_handles.push_back(
      mojo::ScopedHandle::From(consumer.Pass()));
  stashed_objects.push_back(stashed_object.Pass());
  stash_service_->AddToStash(stashed_objects.Pass());
  stashed_objects = RetrieveStash();
  ASSERT_EQ(1u, stashed_objects.size());
  EXPECT_EQ("test type", stashed_objects[0]->id);
  ASSERT_EQ(2u, stashed_objects[0]->stashed_handles.size());

  consumer = mojo::ScopedDataPipeConsumerHandle::From(
      stashed_objects[0]->stashed_handles[1].Pass());
  result = mojo::Wait(
      consumer.get(), MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  char data = '\0';
  result = mojo::ReadDataRaw(
      consumer.get(), &data, &num_bytes, MOJO_READ_DATA_FLAG_ALL_OR_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(1u, num_bytes);
  EXPECT_EQ('1', data);
}

TEST_F(StashServiceTest, RetrieveWithoutStashing) {
  mojo::Array<StashedObjectPtr> stashed_objects = RetrieveStash();
  ASSERT_TRUE(!stashed_objects.is_null());
  EXPECT_EQ(0u, stashed_objects.size());
}

// Test that a stash service discards stashed objects when the backend no longer
// exists.
TEST_F(StashServiceTest, ServiceWithDeletedBackend) {
  stash_backend_.reset();
  stash_service_.set_error_handler(this);

  mojo::Array<StashedObjectPtr> stashed_objects;
  StashedObjectPtr stashed_object(StashedObject::New());
  stashed_object->id = "test type";
  stashed_object->data.push_back(1);
  mojo::MessagePipe message_pipe;
  stashed_object->stashed_handles.push_back(
      mojo::ScopedHandle::From(message_pipe.handle0.Pass()));
  stashed_objects.push_back(stashed_object.Pass());
  stash_service_->AddToStash(stashed_objects.Pass());
  stashed_objects = RetrieveStash();
  ASSERT_EQ(0u, stashed_objects.size());
  // Check that the stashed handle has been closed.
  MojoResult result =
      mojo::Wait(message_pipe.handle1.get(),
                 MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_READABLE,
                 MOJO_DEADLINE_INDEFINITE);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
}

}  // namespace extensions
