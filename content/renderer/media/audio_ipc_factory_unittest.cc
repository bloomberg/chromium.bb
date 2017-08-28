// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_ipc_factory.h"

#include <string>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "content/renderer/media/audio_message_filter.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace content {

namespace {

const int kRenderFrameId = 0;

std::unique_ptr<base::Thread> MakeIOThread() {
  auto io_thread = base::MakeUnique<base::Thread>("test IO thread");
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  CHECK(io_thread->StartWithOptions(thread_options));
  return io_thread;
}

class FakeRemoteFactory : public mojom::RendererAudioOutputStreamFactory {
 public:
  FakeRemoteFactory() : binding_(this) {}
  ~FakeRemoteFactory() override {}

  void RequestDeviceAuthorization(
      media::mojom::AudioOutputStreamProviderRequest stream_provider,
      int64_t session_id,
      const std::string& device_id,
      RequestDeviceAuthorizationCallback callback) override {
    std::move(callback).Run(
        media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
        media::AudioParameters::UnavailableDeviceParams(), std::string());
    EXPECT_FALSE(on_called_.is_null());
    std::move(on_called_).Run();
  }

  void SetOnCalledCallback(base::OnceClosure on_called) {
    on_called_ = std::move(on_called);
  }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    EXPECT_FALSE(binding_.is_bound());
    binding_.Bind(
        mojo::InterfaceRequest<mojom::RendererAudioOutputStreamFactory>(
            std::move(handle)));
  }

 private:
  mojo::Binding<mojom::RendererAudioOutputStreamFactory> binding_;
  base::OnceClosure on_called_;
};

class FakeAudioIPCDelegate : public media::AudioOutputIPCDelegate {
  void OnError() override {}
  void OnDeviceAuthorized(media::OutputDeviceStatus device_status,
                          const media::AudioParameters& output_params,
                          const std::string& matched_device_id) override {}
  void OnStreamCreated(base::SharedMemoryHandle handle,
                       base::SyncSocket::Handle socket_handle,
                       int length) override {}
  void OnIPCClosed() override {}
};

}  // namespace

class AudioIPCFactoryTest : public testing::Test {
 public:
  AudioIPCFactoryTest() {}
  ~AudioIPCFactoryTest() override {}

  void RequestAuthorizationOnIOThread(
      std::unique_ptr<media::AudioOutputIPC> output_ipc) {
    output_ipc->RequestDeviceAuthorization(&fake_delegate, 0, "",
                                           url::Origin());

    output_ipc->CloseStream();
  }

 private:
  FakeAudioIPCDelegate fake_delegate;
};

TEST_F(AudioIPCFactoryTest, CallFactoryFromIOThread) {
  // This test makes sure that AudioIPCFactory correctly binds the
  // RendererAudioOutputStreamFactoryPtr to the IO thread.
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  auto io_thread = MakeIOThread();

  FakeRemoteFactory remote_factory;
  remote_factory.SetOnCalledCallback(run_loop.QuitWhenIdleClosure());

  service_manager::InterfaceProvider interface_provider;
  service_manager::InterfaceProvider::TestApi(&interface_provider)
      .SetBinderForName(mojom::RendererAudioOutputStreamFactory::Name_,
                        base::BindRepeating(&FakeRemoteFactory::Bind,
                                            base::Unretained(&remote_factory)));

  AudioIPCFactory ipc_factory(nullptr, io_thread->task_runner());

  ipc_factory.MaybeRegisterRemoteFactory(kRenderFrameId, &interface_provider);

  // To make sure that the pointer stored in |ipc_factory| is connected to
  // |remote_factory|, and also that it's bound to |io_thread|, we create an
  // AudioOutputIPC object and request device authorization on the IO thread.
  // This is supposed to call |remote_factory| on the main thread.
  io_thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioIPCFactoryTest::RequestAuthorizationOnIOThread,
                     base::Unretained(this),
                     ipc_factory.CreateAudioOutputIPC(kRenderFrameId)));

  // Wait for call to |remote_factory|:
  run_loop.Run();

  ipc_factory.MaybeDeregisterRemoteFactory(0);

  io_thread.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioIPCFactoryTest, SeveralFactories) {
  // This test simulates having several frames being created and destructed.
  base::MessageLoop message_loop;
  auto io_thread = MakeIOThread();
  const int n_factories = 5;

  std::vector<service_manager::InterfaceProvider> interface_providers(
      n_factories);

  std::vector<FakeRemoteFactory> remote_factories(n_factories);

  for (size_t i = 0; i < n_factories; i++) {
    service_manager::InterfaceProvider::TestApi(&interface_providers[i])
        .SetBinderForName(
            mojom::RendererAudioOutputStreamFactory::Name_,
            base::BindRepeating(&FakeRemoteFactory::Bind,
                                base::Unretained(&remote_factories[i])));
  }

  base::RunLoop().RunUntilIdle();

  AudioIPCFactory ipc_factory(nullptr, io_thread->task_runner());

  for (size_t i = 0; i < n_factories; i++) {
    ipc_factory.MaybeRegisterRemoteFactory(kRenderFrameId + i,
                                           &interface_providers[i]);
  }

  base::RunLoop run_loop;
  remote_factories[0].SetOnCalledCallback(run_loop.QuitWhenIdleClosure());
  io_thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioIPCFactoryTest::RequestAuthorizationOnIOThread,
                     base::Unretained(this),
                     ipc_factory.CreateAudioOutputIPC(kRenderFrameId)));
  run_loop.Run();

  // Do some operation and make sure the internal state isn't messed up:
  ipc_factory.MaybeDeregisterRemoteFactory(1);

  base::RunLoop run_loop2;
  remote_factories[2].SetOnCalledCallback(run_loop2.QuitWhenIdleClosure());
  io_thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioIPCFactoryTest::RequestAuthorizationOnIOThread,
                     base::Unretained(this),
                     ipc_factory.CreateAudioOutputIPC(kRenderFrameId + 2)));
  run_loop2.Run();

  for (size_t i = 0; i < n_factories; i++) {
    if (i == 1)
      continue;
    ipc_factory.MaybeDeregisterRemoteFactory(i);
  }

  io_thread.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioIPCFactoryTest, RegisterDeregisterBackToBack_Deregisters) {
  // This test makes sure that calling Register... followed by Deregister...
  // correctly sequences the registration before the deregistration.
  base::MessageLoop message_loop;
  auto io_thread = MakeIOThread();

  FakeRemoteFactory remote_factory;

  service_manager::InterfaceProvider interface_provider;
  service_manager::InterfaceProvider::TestApi(&interface_provider)
      .SetBinderForName(mojom::RendererAudioOutputStreamFactory::Name_,
                        base::BindRepeating(&FakeRemoteFactory::Bind,
                                            base::Unretained(&remote_factory)));

  AudioIPCFactory ipc_factory(nullptr, io_thread->task_runner());

  ipc_factory.MaybeRegisterRemoteFactory(kRenderFrameId, &interface_provider);
  ipc_factory.MaybeDeregisterRemoteFactory(kRenderFrameId);
  // That there is no factory remaining at destruction is DCHECKed in the
  // AudioIPCFactory destructor.

  base::RunLoop().RunUntilIdle();
  io_thread.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
