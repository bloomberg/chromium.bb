// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/gpu/ipc/common/media_messages.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace media {

static const size_t kOutputFrameSizeInBytes = 1024;
static const gfx::Size kDummyFrameCodedSize(10, 10);

class MockFilteredSender : public gpu::FilteredSender {
 public:
  MOCK_METHOD1(AddFilter, void(IPC::MessageFilter*));
  MOCK_METHOD1(RemoveFilter, void(IPC::MessageFilter*));
  MOCK_METHOD1(Send, bool(IPC::Message*));
};

class MockJpegDecodeAccelerator : public media::JpegDecodeAccelerator {
 public:
  MOCK_METHOD1(Initialize, bool(Client*));
  MOCK_METHOD2(Decode,
               void(const BitstreamBuffer&,
                    const scoped_refptr<media::VideoFrame>&));
  MOCK_METHOD0(IsSupported, bool());
};

// Test fixture for the unit that is created via the constructor of
// class GpuJpegDecodeAccelerator. Uses a FakeJpegDecodeAccelerator to
// simulate the actual decoding without the need for special hardware.
class GpuJpegDecodeAcceleratorTest : public ::testing::Test {
 public:
  GpuJpegDecodeAcceleratorTest() : io_thread_("io") {}
  ~GpuJpegDecodeAcceleratorTest() override{};

  void SetUp() override {
    output_frame_memory_.CreateAndMapAnonymous(kOutputFrameSizeInBytes);
    io_thread_.Start();
    std::vector<GpuJpegDecodeAcceleratorFactoryProvider::CreateAcceleratorCB>
        accelerator_factory_functions;
    accelerator_factory_functions.push_back(
        base::Bind(&GpuJpegDecodeAcceleratorTest::GetMockJpegDecodeAccelerator,
                   base::Unretained(this)));
    system_under_test_ = base::MakeUnique<GpuJpegDecodeAccelerator>(
        &gpu_channel_, io_thread_.task_runner(), accelerator_factory_functions);
  }

  MOCK_METHOD1(GetMockJpegDecodeAccelerator,
               std::unique_ptr<JpegDecodeAccelerator>(
                   scoped_refptr<base::SingleThreadTaskRunner>));

  void SendStubFrame(IPC::MessageFilter* message_filter, int32_t route_id) {
    AcceleratedJpegDecoderMsg_Decode_Params decode_params;
    decode_params.output_video_frame_handle = output_frame_memory_.handle();
    decode_params.output_buffer_size =
        static_cast<uint32_t>(kOutputFrameSizeInBytes);
    decode_params.coded_size = kDummyFrameCodedSize;
    AcceleratedJpegDecoderMsg_Decode message(route_id, decode_params);
    message_filter->OnMessageReceived(message);
  }

 protected:
  base::Thread io_thread_;
  MockFilteredSender gpu_channel_;
  std::unique_ptr<GpuJpegDecodeAccelerator> system_under_test_;
  base::SharedMemory output_frame_memory_;

 private:
  // This is required to allow base::ThreadTaskRunnerHandle::Get() from the
  // test execution thread.
  base::MessageLoop message_loop_;
};

// Tests that the communication for decoding a frame between the caller of
// IPC:MessageFilter and the media::JpegDecodeAccelerator works as expected.
TEST_F(GpuJpegDecodeAcceleratorTest, DecodeFrameCallArrivesAtDecoder) {
  static const int32_t kArbitraryRouteId = 123;
  auto io_task_runner = io_thread_.task_runner();
  auto main_task_runner = base::ThreadTaskRunnerHandle::Get();
  auto decoder = base::MakeUnique<MockJpegDecodeAccelerator>();
  auto* decoder_ptr = decoder.get();
  ON_CALL(*decoder, Initialize(_)).WillByDefault(Return(true));

  IPC::MessageFilter* message_filter = nullptr;
  EXPECT_CALL(*this, GetMockJpegDecodeAccelerator(_))
      .WillOnce(InvokeWithoutArgs([&decoder]() { return std::move(decoder); }));
  EXPECT_CALL(gpu_channel_, AddFilter(_)).WillOnce(SaveArg<0>(&message_filter));
  base::RunLoop run_loop;
  auto response_cb = base::Bind(
      [](base::RunLoop* run_loop,
         scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
         bool succeeded) {
        EXPECT_TRUE(succeeded);
        if (main_task_runner->BelongsToCurrentThread() == false) {
          DLOG(INFO) << "Response arrived on thread other than main thread.";
          main_task_runner->PostTask(FROM_HERE, run_loop->QuitClosure());
          return;
        }
        run_loop->Quit();
      },
      &run_loop, main_task_runner);
  system_under_test_->AddClient(kArbitraryRouteId, response_cb);
  run_loop.Run();
  ASSERT_TRUE(message_filter);

  // Send a AcceleratedJpegDecoderMsg_Decode to the |message_filter| on the
  // io_thread.
  base::RunLoop run_loop2;
  EXPECT_CALL(*decoder_ptr, Decode(_, _));
  io_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GpuJpegDecodeAcceleratorTest::SendStubFrame,
                 base::Unretained(this), message_filter, kArbitraryRouteId),
      run_loop2.QuitClosure());
  run_loop2.Run();
}

}  // namespace media
