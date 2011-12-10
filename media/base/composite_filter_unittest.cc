// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/base/composite_filter.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

class CompositeFilterTest : public testing::Test {
 public:
  CompositeFilterTest();
  virtual ~CompositeFilterTest();

  // Sets up a new CompositeFilter in |composite_|, creates |filter_1_| and
  // |filter_2_|, and adds these filters to |composite_|.
  void SetupAndAdd2Filters();

  // Helper enum that indicates what filter method to call.
  enum MethodToCall {
    PLAY,
    PAUSE,
    FLUSH,
    STOP,
    SEEK,
  };

  // Helper method that adds a filter method call expectation based on the value
  // of |method_to_call|.
  //
  // |method_to_call| - Indicates which method we expect a call for.
  // |filter| - The MockFilter to add the expectation to.
  // |seek_time| - The time to pass to the Seek() call if |method_to_call|
  //               equals SEEK.
  void ExpectFilterCall(MethodToCall method_to_call, MockFilter* filter,
                        base::TimeDelta seek_time);

  // Helper method that calls a filter method based on the value of
  // |method_to_call|.
  //
  // |method_to_call| - Indicates which method to call.
  // |filter| - The Filter to make the method call on.
  // |seek_time| - The time to pass to the Seek() call if |method_to_call|
  //               equals SEEK.
  // |callback| - The callback object to pass to the method.
  // |expected_status| - Some filter methods use a FilterStatusCB instead of
  //                     a Closure. For these methods this function
  //                     creates a FilterStatusCB that makes sure the status
  //                     passed to the callback matches |expected_status| and
  //                     then calls |callback|.
  void DoFilterCall(MethodToCall method_to_call, Filter* filter,
                    base::TimeDelta seek_time,
                    const base::Closure& callback,
                    PipelineStatus expected_status);

  // Creates an expectation sequence based on the value of method_to_call.
  //
  // |method_to_call| - Indicates which method we want a success sequence for.
  // |seek_time| - The time to pass in the Seek() call if |method_to_call|
  //               equals SEEK.
  void ExpectSuccess(MethodToCall method_to_call,
                     base::TimeDelta seek_time = base::TimeDelta());

  // Issue a Play(), Pause(), Flush(), Stop(), or Seek() on the composite and
  // verify all the expected calls on the filters.
  void DoPlay();
  void DoPause();
  void DoFlush();
  void DoStop();
  void DoSeek(base::TimeDelta time);

  // Issue a Play(), Pause(), Flush(), or Seek() and expect the calls to fail
  // with a PIPELINE_ERROR_INVALID_STATE error.
  //
  // |method_to_call| - Indicates whick method to call.
  // |seek_time| - The time to pass to the Seek() call if |method_to_call|
  //               equals SEEK.
  void ExpectInvalidStateFail(MethodToCall method_to_call,
                  base::TimeDelta seek_time = base::TimeDelta());

  // Returns whether |filter_1_callback_| or |filter_1_status_cb_| is set.
  bool HasFilter1Callback() const;

  // Run the callback stored in |filter_1_callback_| or |filter_2_status_cb_|.
  void RunFilter1Callback();

  // Returns whether |filter_2_callback_| or |filter_2_status_cb_| is set.
  bool HasFilter2Callback() const;

  // Run the callback stored in |filter_2_callback_| or |filter_2_status_cb_|.
  void RunFilter2Callback();

 protected:
  MessageLoop message_loop_;

  // The composite object being tested.
  scoped_refptr<CompositeFilter> composite_;

  // First filter added to the composite.
  scoped_refptr<StrictMock<MockFilter> > filter_1_;

  // Callback passed to |filter_1_| during last Play(), Pause(), Flush(),
  // or Stop() call.
  base::Closure filter_1_callback_;

  // Status to pass to |filter_1_status_cb_|.
  PipelineStatus filter_1_status_;

  // Callback passed to |filter_1_| during last Seek() call.
  FilterStatusCB filter_1_status_cb_;

  // Second filter added to the composite.
  scoped_refptr<StrictMock<MockFilter> > filter_2_;

  // Callback passed to |filter_2_| during last Play(), Pause(), Flush(),
  // Stop(), or Seek() call.
  base::Closure filter_2_callback_;

  // Status to pass to |filter_2_status_cb_|.
  PipelineStatus filter_2_status_;

  // Callback passed to |filter_2_| during last Seek() call.
  FilterStatusCB filter_2_status_cb_;

  // FilterHost implementation passed to |composite_| via set_host().
  scoped_ptr<StrictMock<MockFilterHost> > mock_filter_host_;

  DISALLOW_COPY_AND_ASSIGN(CompositeFilterTest);
};

CompositeFilterTest::CompositeFilterTest() :
    composite_(new CompositeFilter(&message_loop_)),
    filter_1_status_(PIPELINE_OK),
    filter_2_status_(PIPELINE_OK),
    mock_filter_host_(new StrictMock<MockFilterHost>()) {
}

CompositeFilterTest::~CompositeFilterTest() {}

void CompositeFilterTest::SetupAndAdd2Filters() {
  mock_filter_host_.reset(new StrictMock<MockFilterHost>());
  composite_ = new CompositeFilter(&message_loop_);
  composite_->set_host(mock_filter_host_.get());

  // Setup |filter_1_| and arrange for methods to set
  // |filter_1_callback_| when they are called.
  filter_1_ = new StrictMock<MockFilter>();
  filter_1_callback_.Reset();
  filter_1_status_ = PIPELINE_OK;
  filter_1_status_cb_.Reset();
  ON_CALL(*filter_1_, Play(_))
      .WillByDefault(SaveArg<0>(&filter_1_callback_));
  ON_CALL(*filter_1_, Pause(_))
      .WillByDefault(SaveArg<0>(&filter_1_callback_));
  ON_CALL(*filter_1_, Flush(_))
      .WillByDefault(SaveArg<0>(&filter_1_callback_));
  ON_CALL(*filter_1_, Stop(_))
      .WillByDefault(SaveArg<0>(&filter_1_callback_));
  ON_CALL(*filter_1_, Seek(_,_))
      .WillByDefault(SaveArg<1>(&filter_1_status_cb_));

  // Setup |filter_2_| and arrange for methods to set
  // |filter_2_callback_| when they are called.
  filter_2_ = new StrictMock<MockFilter>();
  filter_2_callback_.Reset();
  filter_2_status_ = PIPELINE_OK;
  filter_2_status_cb_.Reset();
  ON_CALL(*filter_2_, Play(_))
      .WillByDefault(SaveArg<0>(&filter_2_callback_));
  ON_CALL(*filter_2_, Pause(_))
      .WillByDefault(SaveArg<0>(&filter_2_callback_));
  ON_CALL(*filter_2_, Flush(_))
      .WillByDefault(SaveArg<0>(&filter_2_callback_));
  ON_CALL(*filter_2_, Stop(_))
      .WillByDefault(SaveArg<0>(&filter_2_callback_));
  ON_CALL(*filter_2_, Seek(_,_))
      .WillByDefault(SaveArg<1>(&filter_2_status_cb_));

  composite_->AddFilter(filter_1_);
  composite_->AddFilter(filter_2_);
}

void CompositeFilterTest::ExpectFilterCall(MethodToCall method_to_call,
                                           MockFilter* filter,
                                           base::TimeDelta seek_time) {
  switch(method_to_call) {
    case PLAY:
      EXPECT_CALL(*filter, Play(_));
      break;
    case PAUSE:
      EXPECT_CALL(*filter, Pause(_));
      break;
    case FLUSH:
      EXPECT_CALL(*filter, Flush(_));
      break;
    case STOP:
      EXPECT_CALL(*filter, Stop(_));
      break;
    case SEEK:
      EXPECT_CALL(*filter, Seek(seek_time, _));
      break;
  };
}

void OnStatusCB(PipelineStatus expected_status, const base::Closure& callback,
                PipelineStatus status) {
  EXPECT_EQ(status, expected_status);
  callback.Run();
}

void CompositeFilterTest::DoFilterCall(MethodToCall method_to_call,
                                       Filter* filter,
                                       base::TimeDelta seek_time,
                                       const base::Closure& callback,
                                       PipelineStatus expected_status) {
  filter_1_status_ = expected_status;
  filter_2_status_ = expected_status;

  switch(method_to_call) {
    case PLAY:
      filter->Play(callback);
      break;
    case PAUSE:
      filter->Pause(callback);
      break;
    case FLUSH:
      filter->Flush(callback);
      break;
    case STOP:
      filter->Stop(callback);
      break;
    case SEEK:
      filter->Seek(seek_time, base::Bind(&OnStatusCB, expected_status,
                                         callback));
      break;
  };
}

void CompositeFilterTest::ExpectSuccess(MethodToCall method_to_call,
                                        base::TimeDelta seek_time) {
  InSequence seq;

  bool is_parallel_call = (method_to_call == FLUSH);

  ExpectFilterCall(method_to_call, filter_1_.get(), seek_time);

  if (is_parallel_call) {
    ExpectFilterCall(method_to_call, filter_2_.get(), seek_time);
  }

  // Make method call on the composite.
  StrictMock<MockClosure>* callback = new StrictMock<MockClosure>();
  DoFilterCall(method_to_call, composite_.get(), seek_time,
               base::Bind(&MockClosure::Run, callback),
               PIPELINE_OK);

  if (is_parallel_call) {
    // Make sure both filters have their callbacks set.
    EXPECT_TRUE(HasFilter1Callback());
    EXPECT_TRUE(HasFilter2Callback());

    RunFilter1Callback();
  } else {
    // Make sure that only |filter_1_| has its callback set.
    EXPECT_TRUE(HasFilter1Callback());
    EXPECT_FALSE(HasFilter2Callback());

    ExpectFilterCall(method_to_call, filter_2_.get(), seek_time);

    RunFilter1Callback();

    // Verify that |filter_2_| was called by checking the callback pointer.
    EXPECT_TRUE(HasFilter2Callback());
  }

  EXPECT_CALL(*callback, Run());

  RunFilter2Callback();
}

void CompositeFilterTest::DoPlay() {
  ExpectSuccess(PLAY);
}

void CompositeFilterTest::DoPause() {
  ExpectSuccess(PAUSE);
}

void CompositeFilterTest::DoFlush() {
  ExpectSuccess(FLUSH);
}

void CompositeFilterTest::DoStop() {
  ExpectSuccess(STOP);
}

void CompositeFilterTest::DoSeek(base::TimeDelta time) {
  ExpectSuccess(SEEK, time);
}

void CompositeFilterTest::ExpectInvalidStateFail(MethodToCall method_to_call,
                                     base::TimeDelta seek_time) {
  InSequence seq;

  if (method_to_call != SEEK) {
    EXPECT_CALL(*mock_filter_host_, SetError(PIPELINE_ERROR_INVALID_STATE))
        .WillOnce(Return());
  }

  DoFilterCall(method_to_call, composite_, seek_time, NewExpectedClosure(),
               PIPELINE_ERROR_INVALID_STATE);

  // Make sure that neither of the filters were called by
  // verifying that the callback pointers weren't set.
  EXPECT_FALSE(HasFilter1Callback());
  EXPECT_FALSE(HasFilter2Callback());
}

bool CompositeFilterTest::HasFilter1Callback() const {
  CHECK(filter_1_callback_.is_null() || filter_1_status_cb_.is_null());
  return !filter_1_callback_.is_null() || !filter_1_status_cb_.is_null();
}

void CompositeFilterTest::RunFilter1Callback() {
  EXPECT_TRUE(HasFilter1Callback());

  if (!filter_1_status_cb_.is_null()) {
    ResetAndRunCB(&filter_1_status_cb_, filter_1_status_);
    filter_1_status_ = PIPELINE_OK;
    return;
  }

  EXPECT_TRUE(!filter_1_callback_.is_null());
  base::Closure callback = filter_1_callback_;
  filter_1_callback_.Reset();
  callback.Run();
}

bool CompositeFilterTest::HasFilter2Callback() const {
  CHECK(filter_2_callback_.is_null() || filter_2_status_cb_.is_null());
  return !filter_2_callback_.is_null() || !filter_2_status_cb_.is_null();
}

void CompositeFilterTest::RunFilter2Callback() {
  EXPECT_TRUE(HasFilter2Callback());

  if (!filter_2_status_cb_.is_null()) {
    ResetAndRunCB(&filter_2_status_cb_, filter_2_status_);
    filter_2_status_ = PIPELINE_OK;
    return;
  }

  EXPECT_FALSE(filter_2_callback_.is_null());
  base::Closure callback = filter_2_callback_;
  filter_2_callback_.Reset();
  callback.Run();
}

// Test AddFilter() failure cases.
TEST_F(CompositeFilterTest, TestAddFilterFailCases) {
  // Test adding a null pointer.
  EXPECT_FALSE(composite_->AddFilter(NULL));

  scoped_refptr<StrictMock<MockFilter> > filter = new StrictMock<MockFilter>();
  EXPECT_EQ(NULL, filter->host());

  // Test failing because set_host() hasn't been called yet.
  EXPECT_FALSE(composite_->AddFilter(filter));
}

// Test successful AddFilter() cases.
TEST_F(CompositeFilterTest, TestAddFilter) {
  composite_->set_host(mock_filter_host_.get());

  // Add a filter.
  scoped_refptr<StrictMock<MockFilter> > filter = new StrictMock<MockFilter>();
  EXPECT_EQ(NULL, filter->host());

  EXPECT_TRUE(composite_->AddFilter(filter));

  EXPECT_TRUE(filter->host() != NULL);
}

TEST_F(CompositeFilterTest, TestPlay) {
  InSequence sequence;

  SetupAndAdd2Filters();

  // Verify successful call to Play().
  DoPlay();

  // At this point we are now in the kPlaying state.

  // Try calling Play() again to make sure that we simply get a callback.
  // We are already in the Play() state so there is no point calling the
  // filters.
  composite_->Play(NewExpectedClosure());

  // Verify that neither of the filter callbacks were set.
  EXPECT_FALSE(HasFilter1Callback());
  EXPECT_FALSE(HasFilter2Callback());

  // Stop playback.
  DoStop();

  // At this point we should be in the kStopped state.

  // Try calling Stop() again to make sure neither filter is called.
  composite_->Stop(NewExpectedClosure());

  // Verify that neither of the filter callbacks were set.
  EXPECT_FALSE(HasFilter1Callback());
  EXPECT_FALSE(HasFilter2Callback());

  // Try calling Play() again to make sure we get an error.
  ExpectInvalidStateFail(PLAY);
}

// Test errors in the middle of a serial call sequence like Play().
TEST_F(CompositeFilterTest, TestPlayErrors) {
  InSequence sequence;

  SetupAndAdd2Filters();

  EXPECT_CALL(*filter_1_, Play(_));

  // Call Play() on the composite.
  StrictMock<MockClosure>* callback = new StrictMock<MockClosure>();
  composite_->Play(base::Bind(&MockClosure::Run, callback));

  EXPECT_CALL(*filter_2_, Play(_));

  // Run callback to indicate that |filter_1_|'s Play() has completed.
  RunFilter1Callback();

  // At this point Play() has been called on |filter_2_|. Simulate an
  // error by calling SetError() on its FilterHost interface.
  filter_2_->host()->SetError(PIPELINE_ERROR_OUT_OF_MEMORY);

  // Expect error to be reported and "play done" callback to be called.
  EXPECT_CALL(*mock_filter_host_, SetError(PIPELINE_ERROR_OUT_OF_MEMORY));
  EXPECT_CALL(*callback, Run());

  // Run callback to indicate that |filter_2_|'s Play() has completed.
  RunFilter2Callback();

  // Verify that Play/Pause/Flush/Seek fail now that an error occured.
  ExpectInvalidStateFail(PLAY);
  ExpectInvalidStateFail(PAUSE);
  ExpectInvalidStateFail(FLUSH);
  ExpectInvalidStateFail(SEEK);

  // Make sure you can still Stop().
  DoStop();
}

TEST_F(CompositeFilterTest, TestPause) {
  InSequence sequence;

  SetupAndAdd2Filters();

  // Try calling Pause() to make sure we get an error because we aren't in
  // the playing state.
  ExpectInvalidStateFail(PAUSE);

  // Transition to playing state.
  DoPlay();

  // Issue a successful Pause().
  DoPause();

  // At this point we are paused.

  // Try calling Pause() again to make sure that the filters aren't called
  // because we are already in the paused state.
  composite_->Pause(NewExpectedClosure());

  // Verify that neither of the filter callbacks were set.
  EXPECT_FALSE(HasFilter1Callback());
  EXPECT_FALSE(HasFilter2Callback());

  // Verify that we can transition pack to the play state.
  DoPlay();

  // Go back to the pause state.
  DoPause();

  // Transition to the stop state.
  DoStop();

  // Try calling Pause() to make sure we get an error because we aren't in
  // the playing state.
  ExpectInvalidStateFail(PAUSE);
}

// Test errors in the middle of a serial call sequence like Pause().
TEST_F(CompositeFilterTest, TestPauseErrors) {
  InSequence sequence;

  SetupAndAdd2Filters();

  DoPlay();

  EXPECT_CALL(*filter_1_, Pause(_));

  // Call Pause() on the composite.
  StrictMock<MockClosure>* callback = new StrictMock<MockClosure>();
  composite_->Pause(base::Bind(&MockClosure::Run, callback));

  // Simulate an error by calling SetError() on |filter_1_|'s FilterHost
  // interface.
  filter_1_->host()->SetError(PIPELINE_ERROR_OUT_OF_MEMORY);

  // Expect error to be reported and "pause done" callback to be called.
  EXPECT_CALL(*mock_filter_host_, SetError(PIPELINE_ERROR_OUT_OF_MEMORY));
  EXPECT_CALL(*callback, Run());

  RunFilter1Callback();

  // Make sure |filter_2_callback_| was not set.
  EXPECT_FALSE(HasFilter2Callback());

  // Verify that Play/Pause/Flush/Seek fail now that an error occured.
  ExpectInvalidStateFail(PLAY);
  ExpectInvalidStateFail(PAUSE);
  ExpectInvalidStateFail(FLUSH);
  ExpectInvalidStateFail(SEEK);

  // Make sure you can still Stop().
  DoStop();
}

TEST_F(CompositeFilterTest, TestFlush) {
  InSequence sequence;

  SetupAndAdd2Filters();

  // Make sure Flush() works before calling Play().
  DoFlush();

  // Transition to playing state.
  DoPlay();

  // Call Flush() to make sure we get an error because we are in
  // the playing state.
  ExpectInvalidStateFail(FLUSH);

  // Issue a successful Pause().
  DoPause();

  // Make sure Flush() works after pausing.
  DoFlush();

  // Verify that we can transition back to the play state.
  DoPlay();

  // Transition to the stop state.
  DoStop();

  // Try calling Flush() to make sure we get an error because we are stopped.
  ExpectInvalidStateFail(FLUSH);
}

// Test errors in the middle of a parallel call sequence like Flush().
TEST_F(CompositeFilterTest, TestFlushErrors) {
  InSequence sequence;

  SetupAndAdd2Filters();

  EXPECT_CALL(*filter_1_, Flush(_));
  EXPECT_CALL(*filter_2_, Flush(_));

  // Call Flush() on the composite.
  StrictMock<MockClosure>* callback = new StrictMock<MockClosure>();
  composite_->Flush(base::Bind(&MockClosure::Run, callback));

  // Simulate an error by calling SetError() on |filter_1_|'s FilterHost
  // interface.
  filter_1_->host()->SetError(PIPELINE_ERROR_OUT_OF_MEMORY);

  RunFilter1Callback();

  // Expect error to be reported and "pause done" callback to be called.
  EXPECT_CALL(*mock_filter_host_, SetError(PIPELINE_ERROR_OUT_OF_MEMORY));
  EXPECT_CALL(*callback, Run());

  RunFilter2Callback();

  // Verify that Play/Pause/Flush/Seek fail now that an error occured.
  ExpectInvalidStateFail(PLAY);
  ExpectInvalidStateFail(PAUSE);
  ExpectInvalidStateFail(FLUSH);
  ExpectInvalidStateFail(SEEK);

  // Make sure you can still Stop().
  DoStop();
}

TEST_F(CompositeFilterTest, TestSeek) {
  InSequence sequence;

  SetupAndAdd2Filters();

  // Verify that seek is allowed to be called before a Play() call.
  DoSeek(base::TimeDelta::FromSeconds(5));

  // Verify we can issue a Play() after the Seek().
  DoPlay();

  // Try calling Seek() while playing to make sure we get an error.
  ExpectInvalidStateFail(SEEK);

  // Transition to paused state.
  DoPause();

  // Verify that seek is allowed after pausing.
  DoSeek(base::TimeDelta::FromSeconds(5));

  // Verify we can still play again.
  DoPlay();

  // Stop playback.
  DoStop();

  // Try calling Seek() to make sure we get an error.
  ExpectInvalidStateFail(SEEK);
}

TEST_F(CompositeFilterTest, TestStop) {
  InSequence sequence;

  // Test Stop() before any other call.
  SetupAndAdd2Filters();
  DoStop();

  // Test error during Stop() sequence.
  SetupAndAdd2Filters();

  EXPECT_CALL(*filter_1_, Stop(_));

  StrictMock<MockClosure>* callback = new StrictMock<MockClosure>();
  composite_->Stop(base::Bind(&MockClosure::Run, callback));

  // Have |filter_1_| signal an error.
  filter_1_->host()->SetError(PIPELINE_ERROR_READ);

  EXPECT_CALL(*filter_2_, Stop(_));

  RunFilter1Callback();

  EXPECT_CALL(*callback, Run());

  RunFilter2Callback();
}

// Test stopping in the middle of a serial call sequence.
TEST_F(CompositeFilterTest, TestStopWhilePlayPending) {
  InSequence sequence;

  SetupAndAdd2Filters();

  EXPECT_CALL(*filter_1_, Play(_));

  StrictMock<MockClosure>* callback = new StrictMock<MockClosure>();
  composite_->Play(base::Bind(&MockClosure::Run, callback));

  // Note: Play() is pending on |filter_1_| right now.

  callback = new StrictMock<MockClosure>();
  composite_->Stop(base::Bind(&MockClosure::Run, callback));

  EXPECT_CALL(*filter_1_, Stop(_));

  // Run |filter_1_|'s callback again to indicate Play() has completed.
  RunFilter1Callback();

  EXPECT_CALL(*filter_2_, Stop(_));

  // Run |filter_1_|'s callback again to indicate Stop() has completed.
  RunFilter1Callback();

  EXPECT_CALL(*callback, Run());

  // Run |filter_2_|'s callback to indicate Stop() has completed.
  RunFilter2Callback();
}

// Test stopping in the middle of a parallel call sequence.
TEST_F(CompositeFilterTest, TestStopWhileFlushPending) {
  InSequence sequence;

  SetupAndAdd2Filters();

  EXPECT_CALL(*filter_1_, Flush(_));
  EXPECT_CALL(*filter_2_, Flush(_));

  StrictMock<MockClosure>* callback = new StrictMock<MockClosure>();
  composite_->Flush(base::Bind(&MockClosure::Run, callback));

  // Note: |filter_1_| and |filter_2_| have pending Flush() calls at this point.

  callback = new StrictMock<MockClosure>();
  composite_->Stop(base::Bind(&MockClosure::Run, callback));

  // Run callback to indicate that |filter_1_|'s Flush() has completed.
  RunFilter1Callback();

  EXPECT_CALL(*filter_1_, Stop(_));

  // Run callback to indicate that |filter_2_|'s Flush() has completed.
  RunFilter2Callback();

  EXPECT_CALL(*filter_2_, Stop(_));

  // Run callback to indicate that |filter_1_|'s Stop() has completed.
  RunFilter1Callback();

  EXPECT_CALL(*callback, Run());

  // Run callback to indicate that |filter_2_|'s Stop() has completed.
  RunFilter2Callback();
}

TEST_F(CompositeFilterTest, TestErrorWhilePlaying) {
  InSequence sequence;

  SetupAndAdd2Filters();

  // Simulate an error on |filter_2_| while in kCreated state. This
  // can happen if an error occurs during filter initialization.
  EXPECT_CALL(*mock_filter_host_, SetError(PIPELINE_ERROR_OUT_OF_MEMORY));
  filter_2_->host()->SetError(PIPELINE_ERROR_OUT_OF_MEMORY);

  DoPlay();

  // Simulate an error on |filter_2_| while playing.
  EXPECT_CALL(*mock_filter_host_, SetError(PIPELINE_ERROR_OUT_OF_MEMORY));
  filter_2_->host()->SetError(PIPELINE_ERROR_OUT_OF_MEMORY);

  DoPause();

  // Simulate an error on |filter_2_| while paused.
  EXPECT_CALL(*mock_filter_host_, SetError(PIPELINE_ERROR_NETWORK));
  filter_2_->host()->SetError(PIPELINE_ERROR_NETWORK);

  DoStop();

  // Verify that errors are not passed to |mock_filter_host_|
  // after Stop() has been called.
  filter_2_->host()->SetError(PIPELINE_ERROR_NETWORK);
}

// Make sure that state transitions act as expected even
// if the composite doesn't contain any filters.
TEST_F(CompositeFilterTest, TestEmptyComposite) {
  InSequence sequence;

  composite_->set_host(mock_filter_host_.get());

  // Issue a Play() and expect no errors.
  composite_->Play(NewExpectedClosure());

  // Issue a Pause() and expect no errors.
  composite_->Pause(NewExpectedClosure());

  // Issue a Flush() and expect no errors.
  composite_->Flush(NewExpectedClosure());

  // Issue a Seek() and expect no errors.
  composite_->Seek(base::TimeDelta::FromSeconds(5),
                   NewExpectedStatusCB(PIPELINE_OK));

  // Issue a Play() and expect no errors.
  composite_->Play(NewExpectedClosure());

  // Issue a Stop() and expect no errors.
  composite_->Stop(NewExpectedClosure());
}

}  // namespace media
