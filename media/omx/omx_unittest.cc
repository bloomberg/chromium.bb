// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openmax/il/OMX_Component.h"
#include "third_party/openmax/il/OMX_Core.h"

namespace media {

// Defines the maximum number of buffers created for I/O ports.
static const int kMaxBufferNum = 256;

template <typename T>
static void ResetHeader(T* param) {
  memset(param, 0, sizeof(T));
  // TODO(hclam): Make this version number configurable.
  param->nVersion.nVersion = 0x00000101;
  param->nSize = sizeof(T);
}

class OmxTest : public testing::Test {
 public:
  OmxTest()
      : handle_(NULL),
        event_(false, false),
        empty_buffer_(false, false),
        fill_buffer_(false, false),
        last_event_type_(OMX_EventMax),
        last_event_data1_(0),
        last_event_data2_(0) {
    memset(input_buffers_, 0, sizeof(input_buffers_));
    memset(output_buffers_, 0, sizeof(output_buffers_));
  }

 protected:
  virtual void SetUp() {
    // Initialize OpenMAX.
    EXPECT_EQ(OMX_ErrorNone, OMX_Init());
  }

  virtual void TearDown() {
    EXPECT_EQ(OMX_ErrorNone, OMX_Deinit());
  }

  void InitComponent(std::string component_name) {
    // TODO(hclam): Remove static when bug in driver is fixed.
    static OMX_CALLBACKTYPE callback = { &EventHandler,
                                         &EmptyBufferCallback,
                                         &FillBufferCallback };

    OMX_ERRORTYPE omxresult = OMX_GetHandle(
        (void**)&handle_,
        const_cast<OMX_STRING>(component_name.c_str()),
        this, &callback);
    EXPECT_EQ(OMX_ErrorNone, omxresult);
    CHECK(handle_);
  }

  void DeinitComponent() {
    if (handle_)
      OMX_FreeHandle(handle_);
  }

  void AllocateBuffers(int port) {
    int count = 0;
    int size = 0;
    OMX_BUFFERHEADERTYPE** buffers = NULL;
    if (port == input_port_) {
      count = input_buffer_count_;
      size = input_buffer_size_;
      buffers = input_buffers_;
    } else if (port == output_port_) {
      count = output_buffer_count_;
      size = output_buffer_size_;
      buffers = output_buffers_;
    } else {
      NOTREACHED() << "Not a valid port";
    }
    for (int i = 0; i < count; ++i) {
      EXPECT_EQ(OMX_ErrorNone,
                OMX_AllocateBuffer(handle_, buffers + i,
                                   port, NULL, size));
    }
  }

  void ReleaseBuffers(int port) {
    int count = 0;
    OMX_BUFFERHEADERTYPE** buffers = NULL;
    if (port == input_port_) {
      count = input_buffer_count_;
      buffers = input_buffers_;
    } else if (port == output_port_) {
      count = output_buffer_count_;
      buffers = output_buffers_;
    } else {
      NOTREACHED() << "Not a valid port";
    }
    for (int i = 0; i < count; ++i) {
      CHECK(buffers[i]);
      EXPECT_EQ(OMX_ErrorNone,
                OMX_FreeBuffer(handle_, port, buffers[i]));
      buffers[i] = NULL;
    }
  }

  void TransitionLoadedToIdle() {
    EXPECT_EQ(OMX_ErrorNone,
              OMX_SendCommand(handle_, OMX_CommandStateSet,
                              OMX_StateIdle, 0));
    AllocateBuffers(input_port_);
    AllocateBuffers(output_port_);
    event_.Wait();
    EXPECT_EQ(OMX_EventCmdComplete, last_event_type_);
    EXPECT_EQ(OMX_CommandStateSet, last_event_data1_);
    EXPECT_EQ(OMX_StateIdle, last_event_data2_);
  }

  void TransitionIdleToLoaded() {
    EXPECT_EQ(OMX_ErrorNone,
              OMX_SendCommand(handle_, OMX_CommandStateSet,
                              OMX_StateLoaded, 0));
    ReleaseBuffers(input_port_);
    ReleaseBuffers(output_port_);
    event_.Wait();
    EXPECT_EQ(OMX_EventCmdComplete, last_event_type_);
    EXPECT_EQ(OMX_CommandStateSet, last_event_data1_);
    EXPECT_EQ(OMX_StateLoaded, last_event_data2_);
  }

  void TransitionIdleToExecuting() {
    EXPECT_EQ(OMX_ErrorNone,
              OMX_SendCommand(handle_, OMX_CommandStateSet,
                              OMX_StateExecuting, 0));
    event_.Wait();
    EXPECT_EQ(OMX_EventCmdComplete, last_event_type_);
    EXPECT_EQ(OMX_CommandStateSet, last_event_data1_);
    EXPECT_EQ(OMX_StateExecuting, last_event_data2_);
  }

  void TransitionExecutingToIdle() {
    EXPECT_EQ(OMX_ErrorNone,
              OMX_SendCommand(handle_, OMX_CommandStateSet,
                              OMX_StateIdle, 0));
    event_.Wait();
    EXPECT_EQ(OMX_EventCmdComplete, last_event_type_);
    EXPECT_EQ(OMX_CommandStateSet, last_event_data1_);
    EXPECT_EQ(OMX_StateIdle, last_event_data2_);
  }

  void GetComponentsOfRole(std::string role) {
    OMX_U32 roles = 0;
    OMX_U8** component_names = NULL;
    const int kSize = 256;

    LOG(INFO) << "GetComponentsOfRole: " << role;
    EXPECT_EQ(OMX_ErrorNone, OMX_GetComponentsOfRole(
        const_cast<OMX_STRING>(role.c_str()), &roles, 0));

    // TODO(hclam): Should assert the component number.
    LOG(INFO) << "Components: " << roles;

    if (roles) {
      component_names = new OMX_U8*[roles];
      for (size_t i = 0; i < roles; ++i)
        component_names[i] = new OMX_U8[kSize];

      OMX_U32 roles_backup = roles;
      EXPECT_EQ(OMX_ErrorNone,
                OMX_GetComponentsOfRole(
                    const_cast<OMX_STRING>(role.c_str()),
                    &roles, component_names));
      ASSERT_EQ(roles_backup, roles);

      for (size_t i = 0; i < roles; ++i) {
        LOG(INFO) << "Component name: " << component_names[i];
        delete [] component_names[i];
      }
      delete [] component_names;
    }
  }

  OMX_ERRORTYPE EventHandlerInternal(
      OMX_HANDLETYPE component, OMX_EVENTTYPE event,
      OMX_U32 data1, OMX_U32 data2, OMX_PTR event_data) {
    last_event_type_ = event;
    last_event_data1_ = static_cast<int>(data1);
    last_event_data2_ = static_cast<int>(data2);
    // TODO(hclam): Save |event_data|.
    event_.Signal();
    return OMX_ErrorNone;
  }

  OMX_ERRORTYPE EmptyBufferCallbackInternal(
      OMX_HANDLETYPE component, OMX_BUFFERHEADERTYPE* buffer) {
    // TODO(hclam): Add code here.
    empty_buffer_.Signal();
    return OMX_ErrorNone;
  }

  OMX_ERRORTYPE FillBufferCallbackInternal(
      OMX_HANDLETYPE component, OMX_BUFFERHEADERTYPE* buffer) {
    // TODO(hclam): Add code here.
    fill_buffer_.Signal();
    return OMX_ErrorNone;
  }

  // Static callback methods.
  static OMX_ERRORTYPE EventHandler(
      OMX_HANDLETYPE component, OMX_PTR priv_data,
      OMX_EVENTTYPE event, OMX_U32 data1, OMX_U32 data2,
      OMX_PTR event_data) {
    return static_cast<OmxTest*>(priv_data)
        ->EventHandlerInternal(component,
                               event, data1, data2, event_data);
  }

  static OMX_ERRORTYPE EmptyBufferCallback(
      OMX_HANDLETYPE component, OMX_PTR priv_data,
      OMX_BUFFERHEADERTYPE* buffer) {
    return static_cast<OmxTest*>(priv_data)
        ->EmptyBufferCallbackInternal(component, buffer);
  }

  static OMX_ERRORTYPE FillBufferCallback(
      OMX_HANDLETYPE component, OMX_PTR priv_data,
      OMX_BUFFERHEADERTYPE* buffer) {
    return static_cast<OmxTest*>(priv_data)
        ->FillBufferCallbackInternal(component, buffer);
  }

  OMX_COMPONENTTYPE* handle_;
  int input_port_;
  int output_port_;
  int input_buffer_count_;
  int input_buffer_size_;
  int output_buffer_count_;
  int output_buffer_size_;
  OMX_BUFFERHEADERTYPE* input_buffers_[kMaxBufferNum];
  OMX_BUFFERHEADERTYPE* output_buffers_[kMaxBufferNum];

  base::WaitableEvent event_;
  base::WaitableEvent empty_buffer_;
  base::WaitableEvent fill_buffer_;

  OMX_EVENTTYPE last_event_type_;
  int last_event_data1_;
  int last_event_data2_;
};

class OmxVideoDecoderTest : public OmxTest {
 protected:
  void Configure(OMX_VIDEO_CODINGTYPE codec,
                 int width, int height) {
    // Obtain port IDs.
    OMX_PORT_PARAM_TYPE port_param;
    ResetHeader(&port_param);
    EXPECT_EQ(OMX_ErrorNone,
              OMX_GetParameter(handle_,
                               OMX_IndexParamVideoInit,
                               &port_param));

    input_port_ = port_param.nStartPortNumber;
    output_port_ = port_param.nStartPortNumber + 1;
    LOG(INFO) << "Input port number: " << input_port_;
    LOG(INFO) << "Output port number: " << output_port_;

    // Get and set parameters for input port.
    LOG(INFO) << "Input port width: " << width;
    LOG(INFO) << "Input port height: " << height;
    LOG(INFO) << "Input port codec: " << codec;
    OMX_PARAM_PORTDEFINITIONTYPE port_format;
    ResetHeader(&port_format);
    port_format.nPortIndex = input_port_;
    EXPECT_EQ(OMX_ErrorNone,
              OMX_GetParameter(handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format));
    EXPECT_EQ(OMX_DirInput, port_format.eDir);
    port_format.format.video.eCompressionFormat = codec;
    port_format.format.video.nFrameWidth  = width;
    port_format.format.video.nFrameHeight = height;
    EXPECT_EQ(OMX_ErrorNone,
              OMX_SetParameter(handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format));

    // TODO(hclam): Add configurations to output port.

    // Get Parameters for input port.
    ResetHeader(&port_format);
    port_format.nPortIndex = input_port_;
    EXPECT_EQ(OMX_ErrorNone,
              OMX_GetParameter(handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format));
    EXPECT_EQ(OMX_DirInput, port_format.eDir);
    input_buffer_count_ = port_format.nBufferCountMin;
    input_buffer_size_ = port_format.nBufferSize;
    CHECK(input_buffer_count_ < kMaxBufferNum);

    // Get parameters for output port.
    ResetHeader(&port_format);
    port_format.nPortIndex = output_port_;
    EXPECT_EQ(OMX_ErrorNone,
              OMX_GetParameter(handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format));
    EXPECT_EQ(OMX_DirOutput, port_format.eDir);
    output_buffer_count_ = port_format.nBufferCountMin;
    output_buffer_size_ = port_format.nBufferSize;
    CHECK(output_buffer_count_ < kMaxBufferNum);

    LOG(INFO) << "Input buffer count: " << input_buffer_count_;
    LOG(INFO) << "Input buffer size: " << input_buffer_size_;
    LOG(INFO) << "Output buffer count: " << output_buffer_count_;
    LOG(INFO) << "Output buffer size: " << output_buffer_size_;
  }

  std::string component() {
    return CommandLine::ForCurrentProcess()
        ->GetSwitchValueASCII("video-decoder-component");
  }

  OMX_VIDEO_CODINGTYPE codec() {
    std::string codec = CommandLine::ForCurrentProcess()
        ->GetSwitchValueASCII("video-decoder-codec");
    if (codec == "h264")
      return OMX_VIDEO_CodingAVC;
    return OMX_VIDEO_CodingAutoDetect;
  }
};

TEST_F(OmxTest, SimpleInit) {
  // A empty test case will test basic init/deinit of OpenMAX library.
}

TEST_F(OmxTest, GetComponentsOfRole) {
  // Roles video decoders.
  GetComponentsOfRole("video_decoder.avc");
  GetComponentsOfRole("video_decoder.mpeg4");
  GetComponentsOfRole("video_decoder.vc1");

  // TODO(hclam): Add roles of encoders.
}

TEST_F(OmxVideoDecoderTest, GetHandle) {
  // TODO(hclam): Should use GetComponentsOfRole instead.
  InitComponent(component());
  DeinitComponent();
}

TEST_F(OmxVideoDecoderTest, Configuration) {
  InitComponent(component());
  // TODO(hclam): Make resolution configurable.
  Configure(codec(), 1024, 768);
  DeinitComponent();
}

TEST_F(OmxVideoDecoderTest, TransitionToIdle) {
  InitComponent(component());
  Configure(codec(), 1024, 768);
  TransitionLoadedToIdle();
  TransitionIdleToLoaded();
  DeinitComponent();
}

TEST_F(OmxVideoDecoderTest, FreeHandleWhenIdle) {
  InitComponent(component());
  Configure(codec(), 1024, 768);
  TransitionLoadedToIdle();
  DeinitComponent();
}

TEST_F(OmxVideoDecoderTest, TransitionToExecuting) {
  InitComponent(component());
  Configure(codec(), 1024, 768);
  TransitionLoadedToIdle();
  TransitionIdleToExecuting();
  TransitionExecutingToIdle();
  TransitionIdleToLoaded();
  DeinitComponent();
}

TEST_F(OmxVideoDecoderTest, FreeHandleWhenExecuting) {
  InitComponent(component());
  Configure(codec(), 1024, 768);
  TransitionLoadedToIdle();
  TransitionIdleToExecuting();
  DeinitComponent();
}

TEST_F(OmxVideoDecoderTest, CallbacksAreCopied) {
  // Allocate a callback struct on stack and clear it with zero.
  // This make sure OpenMAX library will copy the content of the
  // struct.
  OMX_CALLBACKTYPE callback = { &EventHandler,
                                &EmptyBufferCallback,
                                &FillBufferCallback };

  OMX_ERRORTYPE omxresult = OMX_GetHandle(
      (void**)&handle_,
      const_cast<OMX_STRING>(component().c_str()),
      this, &callback);
  EXPECT_EQ(OMX_ErrorNone, omxresult);
  CHECK(handle_);
  memset(&callback, 0, sizeof(callback));

  // Then configure the component as usual.
  Configure(codec(), 1024, 768);
  TransitionLoadedToIdle();
  TransitionIdleToExecuting();
  TransitionExecutingToIdle();
  TransitionIdleToLoaded();
  DeinitComponent();
}

}  // namespace media
