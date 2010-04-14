// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _CRT_SECURE_NO_WARNINGS

#include <deque>

#include "base/callback.h"
#include "base/message_loop.h"
#include "media/base/mock_filters.h"
#include "media/omx/mock_omx.h"
#include "media/omx/omx_codec.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace {

const int kBufferCount = 3;
const int kBufferSize = 4096;
const char* kRoleName = "awsome";
const char* kComponentName = "OMX.google.mock.awsome";

}  // namespace

namespace media {

ACTION(ReturnComponentName) {
  strcpy(((char**)arg2)[0], kComponentName);
}

ACTION(GetHandle) {
  *arg0 = MockOmx::get()->component();
  MockOmx::get()->component()->pApplicationPrivate = arg2;
  memcpy(MockOmx::get()->callbacks(), arg3, sizeof(OMX_CALLBACKTYPE));
}

ACTION(GetParameterVideoInit) {
  ((OMX_PORT_PARAM_TYPE*)arg1)->nStartPortNumber = 0;
}

ACTION(GetParameterPortDefinition) {
  OMX_PARAM_PORTDEFINITIONTYPE* port_format =
      (OMX_PARAM_PORTDEFINITIONTYPE*)arg1;
  CHECK(port_format->nPortIndex == 0 || port_format->nPortIndex == 1);
  if (port_format->nPortIndex == 0)
    port_format->eDir = OMX_DirInput;
  else
    port_format->eDir = OMX_DirOutput;
  port_format->nBufferCountMin = kBufferCount;
  port_format->nBufferSize = kBufferSize;
}

ACTION(AllocateBuffer) {
  *arg0 = new OMX_BUFFERHEADERTYPE();
  memset(*arg0, 0, sizeof(OMX_BUFFERHEADERTYPE));
  (*arg0)->pBuffer = new uint8[kBufferSize];
}

ACTION(UseBuffer) {
  *arg0 = new OMX_BUFFERHEADERTYPE();
  memset(*arg0, 0, sizeof(OMX_BUFFERHEADERTYPE));
}

ACTION(FreeBuffer) {
  delete [] arg1->pBuffer;
  delete arg1;
}

ACTION_P2(SendEvent, event, data1) {
  // TODO(hclam): pass data2 and event data.
  (*MockOmx::get()->callbacks()->EventHandler)(
      MockOmx::get()->component(),
      MockOmx::get()->component()->pApplicationPrivate,
      event, static_cast<OMX_U32>(data1), 0, NULL);
}

ACTION(FillBuffer) {
  arg0->nFlags = 0;
  arg0->nFilledLen = kBufferSize;
  (*MockOmx::get()->callbacks()->FillBufferDone)(
      MockOmx::get()->component(),
      MockOmx::get()->component()->pApplicationPrivate,
      arg0);
}

ACTION(FillEosBuffer) {
  arg0->nFlags = OMX_BUFFERFLAG_EOS;
  arg0->nFilledLen = 0;
  (*MockOmx::get()->callbacks()->FillBufferDone)(
      MockOmx::get()->component(),
      MockOmx::get()->component()->pApplicationPrivate,
      arg0);
}

class MockOmxConfigurator : public OmxConfigurator {
 public:
  MockOmxConfigurator()
      : OmxConfigurator(MediaFormat(), MediaFormat()) {}

  MOCK_CONST_METHOD0(GetRoleName, std::string());
  MOCK_CONST_METHOD3(ConfigureIOPorts,
                     bool(OMX_COMPONENTTYPE* component,
                          OMX_PARAM_PORTDEFINITIONTYPE* input_fef,
                          OMX_PARAM_PORTDEFINITIONTYPE* output_def));
};

class MockOmxOutputSink : public OmxOutputSink {
 public:
  MOCK_CONST_METHOD0(ProvidesEGLImages, bool());
  MOCK_METHOD3(AllocateEGLImages,
               bool(int width, int height,
                    std::vector<EGLImageKHR>* images));
  MOCK_METHOD1(ReleaseEGLImages,
               void(const std::vector<EGLImageKHR>& images));
  MOCK_METHOD2(UseThisBuffer, void(int buffer_id,
                                   OMX_BUFFERHEADERTYPE* buffer));
  MOCK_METHOD1(StopUsingThisBuffer, void(int buffer_id));
  MOCK_METHOD2(BufferReady, void(int buffer_id,
                                 BufferUsedCallback* callback));
};

class OmxCodecTest : public testing::Test {
 public:
  OmxCodecTest ()
      : omx_codec_(new OmxCodec(&message_loop_)) {
    omx_codec_->Setup(&mock_configurator_, &mock_output_sink_);
  }

  ~OmxCodecTest() {
    CHECK(output_units_.size() == 0);
  }

 protected:
  void ExpectSettings() {
    // Return the component name.
    EXPECT_CALL(*MockOmx::get(), GetComponentsOfRole(_, _, IsNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(1),
                        Return(OMX_ErrorNone)));
    EXPECT_CALL(*MockOmx::get(), GetComponentsOfRole(_, _, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(1),
                        ReturnComponentName(),
                        Return(OMX_ErrorNone)));

    // Handle get parameter calls.
    EXPECT_CALL(*MockOmx::get(),
                GetParameter(OMX_IndexParamVideoInit, NotNull()))
        .WillRepeatedly(DoAll(GetParameterVideoInit(), Return(OMX_ErrorNone)));
    EXPECT_CALL(*MockOmx::get(),
                GetParameter(OMX_IndexParamPortDefinition, NotNull()))
        .WillRepeatedly(DoAll(GetParameterPortDefinition(),
                              Return(OMX_ErrorNone)));

    // Ignore all set parameter calls.
    EXPECT_CALL(*MockOmx::get(), SetParameter(_, _))
        .WillRepeatedly(Return(OMX_ErrorNone));

    // Expect calling to configurator once.
    EXPECT_CALL(mock_configurator_,
                ConfigureIOPorts(MockOmx::get()->component(), _, _))
        .WillOnce(Return(true));

    EXPECT_CALL(mock_configurator_, GetRoleName())
        .WillRepeatedly(Return(kRoleName));
  }

  void ExpectToLoaded() {
    InSequence s;

    // Expect initialization.
    EXPECT_CALL(*MockOmx::get(), Init())
        .WillOnce(Return(OMX_ErrorNone));

    // Return the handle.
    EXPECT_CALL(*MockOmx::get(),
                GetHandle(NotNull(), StrEq(kComponentName),
                          NotNull(), NotNull()))
        .WillOnce(DoAll(GetHandle(),
                        Return(OMX_ErrorNone)));
  }

  void ExpectLoadedToIdle() {
    InSequence s;

    // Expect transition to idle.
    EXPECT_CALL(*MockOmx::get(),
                SendCommand(OMX_CommandStateSet, OMX_StateIdle, _))
        .WillOnce(
            DoAll(
                SendEvent(OMX_EventCmdComplete, OMX_CommandStateSet),
                Return(OMX_ErrorNone)));

    // Expect allocation of buffers.
    EXPECT_CALL(*MockOmx::get(),
                UseBuffer(NotNull(), 0, IsNull(), kBufferSize, _))
        .Times(kBufferCount)
        .WillRepeatedly(DoAll(UseBuffer(), Return(OMX_ErrorNone)));

    // Don't support EGL images in this case.
    EXPECT_CALL(mock_output_sink_, ProvidesEGLImages())
        .WillOnce(Return(false));

    // Expect allocation of output buffers and send command complete.
    EXPECT_CALL(*MockOmx::get(),
                AllocateBuffer(NotNull(), 1, IsNull(), kBufferSize))
        .Times(kBufferCount)
        .WillRepeatedly(DoAll(AllocateBuffer(), Return(OMX_ErrorNone)));

    // The allocate output buffers will then be passed to output sink.
    for (int i = 0; i < kBufferCount; ++i) {
      EXPECT_CALL(mock_output_sink_, UseThisBuffer(i, _));
    }
  }

  void ExpectToExecuting() {
    InSequence s;

    // Expect transition to executing.
    EXPECT_CALL(*MockOmx::get(),
                SendCommand(OMX_CommandStateSet, OMX_StateExecuting, _))
        .WillOnce(DoAll(
            SendEvent(OMX_EventCmdComplete, OMX_CommandStateSet),
            Return(OMX_ErrorNone)));

    // Expect initial FillThisBuffer() calls.
    EXPECT_CALL(*MockOmx::get(), FillThisBuffer(NotNull()))
        .Times(kBufferCount)
        .WillRepeatedly(DoAll(FillBuffer(), Return(OMX_ErrorNone)));
  }

  void ExpectToIdle() {
    // Expect going to idle
    EXPECT_CALL(*MockOmx::get(),
                SendCommand(OMX_CommandStateSet, OMX_StateIdle, _))
        .WillOnce(DoAll(
            SendEvent(OMX_EventCmdComplete, OMX_CommandStateSet),
            Return(OMX_ErrorNone)));
  }

  void ExpectIdleToLoaded() {
    InSequence s;

    // Expect transition to loaded.
    EXPECT_CALL(*MockOmx::get(),
                SendCommand(OMX_CommandStateSet, OMX_StateLoaded, _))
        .WillOnce(DoAll(
            SendEvent(OMX_EventCmdComplete, OMX_CommandStateSet),
            Return(OMX_ErrorNone)));

    // Expect free buffer for input port.
    EXPECT_CALL(*MockOmx::get(), FreeBuffer(0, NotNull()))
        .Times(kBufferCount)
        .WillRepeatedly(DoAll(FreeBuffer(), Return(OMX_ErrorNone)));

    // Expect free output buffer.
    for (int i = 0; i < kBufferCount; ++i) {
      EXPECT_CALL(mock_output_sink_, StopUsingThisBuffer(i));
    }

    EXPECT_CALL(*MockOmx::get(), FreeBuffer(1, NotNull()))
        .Times(kBufferCount)
        .WillRepeatedly(DoAll(FreeBuffer(), Return(OMX_ErrorNone)));

    // Report that the sink don't provide EGL images.
    EXPECT_CALL(mock_output_sink_, ProvidesEGLImages())
        .WillOnce(Return(false));
  }

  void ExpectToEmpty() {
    InSequence s;

    EXPECT_CALL(*MockOmx::get(), FreeHandle(MockOmx::get()->component()))
        .WillOnce(Return(OMX_ErrorNone));
    EXPECT_CALL(*MockOmx::get(), Deinit())
        .WillOnce(Return(OMX_ErrorNone));
  }

  // TODO(hclam): Make a more generic about when to stop.
  void ExpectStart() {
    ExpectToLoaded();
    ExpectLoadedToIdle();
    ExpectToExecuting();
  }

  void ExpectStop() {
    ExpectToIdle();
    ExpectIdleToLoaded();
    ExpectToEmpty();
  }

  void ReadCallback(int buffer_id,
                    OmxOutputSink::BufferUsedCallback* callback) {
    output_units_.push_back(std::make_pair(buffer_id, callback));
  }

  void MakeReadRequest() {
    omx_codec_->Read(NewCallback(this, &OmxCodecTest::ReadCallback));
  }

  void SaveFillThisBuffer(OMX_BUFFERHEADERTYPE* buffer) {
    fill_this_buffer_received_.push_back(buffer);
  }

  void ExpectAndSaveFillThisBuffer() {
    EXPECT_CALL(*MockOmx::get(), FillThisBuffer(NotNull()))
        .WillOnce(DoAll(Invoke(this, &OmxCodecTest::SaveFillThisBuffer),
                        Return(OMX_ErrorNone)))
        .RetiresOnSaturation();
  }

  typedef std::pair<int, OmxOutputSink::BufferUsedCallback*> OutputUnit;
  std::deque<OutputUnit> output_units_;
  std::deque<OMX_BUFFERHEADERTYPE*> fill_this_buffer_received_;

  MockOmx mock_omx_;
  MockOmxConfigurator mock_configurator_;
  MockOmxOutputSink mock_output_sink_;

  MessageLoop message_loop_;
  scoped_refptr<OmxCodec> omx_codec_;

  MockFilterCallback stop_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmxCodecTest);
};

TEST_F(OmxCodecTest, SimpleStartAndStop) {
  ExpectSettings();
  ExpectStart();
  omx_codec_->Start();
  message_loop_.RunAllPending();

  EXPECT_CALL(stop_callback_, OnFilterCallback());
  EXPECT_CALL(stop_callback_, OnCallbackDestroyed());
  ExpectStop();
  omx_codec_->Stop(stop_callback_.NewCallback());
  message_loop_.RunAllPending();
}

TEST_F(OmxCodecTest, EndOfStream) {
  ExpectSettings();
  ExpectStart();
  omx_codec_->Start();
  message_loop_.RunAllPending();

  // Make read requests, OmxCodec should have gotten kBufferCount
  // buffers already.
  EXPECT_EQ(0u, output_units_.size());
  for (int i = 0; i < kBufferCount; ++i) {
    MakeReadRequest();
  }
  message_loop_.RunAllPending();
  CHECK(kBufferCount == static_cast<int>(output_units_.size()));

  // Make sure buffers received are in order.
  for (int i = 0; i < kBufferCount; ++i) {
    EXPECT_EQ(i, output_units_[i].first);
    EXPECT_TRUE(output_units_[i].second != NULL);
  }

  // Give buffers back to OmxCodec. OmxCodec will make a new
  // FillThisBuffer() call for each read.
  for (int i = kBufferCount - 1; i >= 0; --i) {
    EXPECT_CALL(*MockOmx::get(), FillThisBuffer(NotNull()))
        .WillOnce(DoAll(FillEosBuffer(), Return(OMX_ErrorNone)))
        .RetiresOnSaturation();
    output_units_[i].second->Run(output_units_[i].first);
    delete output_units_[i].second;
  }
  output_units_.clear();

  // Make some read requests and make sure end-of-stream buffer id
  // are received.
  EXPECT_EQ(0u, output_units_.size());
  for (int i = 0; i < 2 * kBufferCount; ++i) {
    MakeReadRequest();
  }
  message_loop_.RunAllPending();
  EXPECT_EQ(2 * kBufferCount, static_cast<int>(output_units_.size()));

  for (size_t i = 0; i <output_units_.size(); ++i) {
    EXPECT_EQ(OmxCodec::kEosBuffer, output_units_[i].first);
    EXPECT_EQ(NULL, output_units_[i].second);
  }
  output_units_.clear();

  // Stop OmxCodec.
  EXPECT_CALL(stop_callback_, OnFilterCallback());
  EXPECT_CALL(stop_callback_, OnCallbackDestroyed());
  ExpectStop();
  omx_codec_->Stop(stop_callback_.NewCallback());
  message_loop_.RunAllPending();
}

TEST_F(OmxCodecTest, OutputFlowControl) {
  ExpectSettings();
  ExpectStart();
  omx_codec_->Start();
  message_loop_.RunAllPending();

  // Since initial FillThisBuffer() calls are made and fulfilled during
  // start. Reads issued to OmxCodec will be fulfilled now.
  EXPECT_EQ(0u, output_units_.size());
  for (int i = 0; i < kBufferCount; ++i) {
    MakeReadRequest();
  }
  message_loop_.RunAllPending();
  CHECK(kBufferCount == static_cast<int>(output_units_.size()));

  // Make sure buffers received are in order.
  for (int i = 0; i < kBufferCount; ++i) {
    EXPECT_EQ(i, output_units_[i].first);
    EXPECT_TRUE(output_units_[i].second != NULL);
  }

  // Give the buffer back in reverse order.
  for (int i = kBufferCount - 1; i >= 0; --i) {
    ExpectAndSaveFillThisBuffer();
    output_units_[i].second->Run(output_units_[i].first);
    delete output_units_[i].second;
  }
  output_units_.clear();

  // In each loop, perform the following actions:
  // 1. Make a read request to OmxCodec.
  // 2. Fake a response for FillBufferDone().
  // 3. Expect read response received.
  // 4. Give the buffer read back to OmxCodec.
  // 5. Expect a FillThisBuffer() is called to OpenMAX.
  for (int i = 0; i < kBufferCount; ++i) {
    // 1. First make a read request.
    MakeReadRequest();

    // 2. Then fake a response from OpenMAX.
    OMX_BUFFERHEADERTYPE* buffer = fill_this_buffer_received_.front();
    fill_this_buffer_received_.pop_front();
    buffer->nFlags = 0;
    buffer->nFilledLen = kBufferSize;
    (*MockOmx::get()->callbacks()->FillBufferDone)(
        MockOmx::get()->component(),
        MockOmx::get()->component()->pApplicationPrivate,
        buffer);

    // Make sure actions are completed.
    message_loop_.RunAllPending();

    // 3. Expect read response received.
    // The above action will cause a read callback be called and we should
    // receive one buffer now. Also expect the buffer id be received in
    // reverse order.
    EXPECT_EQ(1u, output_units_.size());
    EXPECT_EQ(kBufferCount - i - 1, output_units_.front().first);

    // 4. Since a buffer is given back to OmxCodec. A FillThisBuffer() is called
    // to OmxCodec.
    EXPECT_CALL(*MockOmx::get(), FillThisBuffer(NotNull()))
        .WillOnce(Return(OMX_ErrorNone))
        .RetiresOnSaturation();

    // 5. Give this buffer back to OmxCodec.
    output_units_.front().second->Run(output_units_.front().first);
    delete output_units_.front().second;
    output_units_.pop_front();

    // Make sure actions are completed.
    message_loop_.RunAllPending();
  }

  // Now issue kBufferCount reads to OmxCodec.
  EXPECT_EQ(0u, output_units_.size());

  // Stop OmxCodec.
  EXPECT_CALL(stop_callback_, OnFilterCallback());
  EXPECT_CALL(stop_callback_, OnCallbackDestroyed());
  ExpectStop();
  omx_codec_->Stop(stop_callback_.NewCallback());
  message_loop_.RunAllPending();
}

// TODO(hclam): Add test case for dynamic port config.
// TODO(hclam): Create a more complicated test case so that read
// requests and reply from FillThisBuffer() arrives out of order.
// TODO(hclam): Add test case for Feed().

}  // namespace media
