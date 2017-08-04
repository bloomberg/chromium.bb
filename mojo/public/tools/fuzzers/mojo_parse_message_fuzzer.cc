// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task_scheduler/task_scheduler.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/tools/fuzzers/fuzz.mojom.h"

/* Dummy implementation of the FuzzInterface. */
class FuzzImpl : public fuzz::mojom::FuzzInterface {
 public:
  explicit FuzzImpl(fuzz::mojom::FuzzInterfaceRequest request)
      : binding_(this, std::move(request)) {}

  void FuzzBasic() override {}

  void FuzzBasicResp(FuzzBasicRespCallback callback) override {
    std::move(callback).Run();
  }

  void FuzzBasicSyncResp(FuzzBasicSyncRespCallback callback) override {
    std::move(callback).Run();
  }

  void FuzzArgs(fuzz::mojom::FuzzStructPtr a,
                fuzz::mojom::FuzzStructPtr b) override {}

  void FuzzArgsResp(fuzz::mojom::FuzzStructPtr a,
                    fuzz::mojom::FuzzStructPtr b,
                    FuzzArgsRespCallback callback) override {
    std::move(callback).Run();
  };

  void FuzzArgsSyncResp(fuzz::mojom::FuzzStructPtr a,
                        fuzz::mojom::FuzzStructPtr b,
                        FuzzArgsSyncRespCallback callback) override {
    std::move(callback).Run();
  };

  ~FuzzImpl() override {}

  /* Expose the binding to the fuzz harness. */
  mojo::Binding<FuzzInterface> binding_;
};

void FuzzMessage(const uint8_t* data, size_t size, base::RunLoop* run) {
  fuzz::mojom::FuzzInterfacePtr fuzz;
  auto impl = base::MakeUnique<FuzzImpl>(MakeRequest(&fuzz));
  auto router = impl->binding_.RouterForTesting();

  /* Create a mojo message with the appropriate payload size. */
  mojo::Message message(0, 0, size, 0, nullptr);
  if (message.data_num_bytes() < size) {
    message.payload_buffer()->Allocate(size - message.data_num_bytes());
  }

  /* Set the raw message data. */
  memcpy(message.mutable_data(), data, size);

  /* Run the message through header validation, payload validation, and
   * dispatch to the impl. */
  router->SimulateReceivingMessageForTesting(&message);

  /* Allow the harness function to return now. */
  run->Quit();
}

/* Environment for the fuzzer. Initializes the mojo EDK and sets up a
 * TaskScheduler, because Mojo messages must be sent and processed from
 * TaskRunners. */
struct Environment {
  Environment() : message_loop(base::MessageLoop::TYPE_UI) {
    base::TaskScheduler::CreateAndStartWithDefaultParams(
        "MojoParseMessageFuzzerProcess");
    mojo::edk::Init();
  }

  /* Message loop to send and handle messages on. */
  base::MessageLoop message_loop;

  /* Suppress mojo validation failure logs. */
  mojo::internal::ScopedSuppressValidationErrorLoggingForTests log_suppression;
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  /* Pass the data along to run on a MessageLoop, and wait for it to finish. */
  base::RunLoop run;
  env->message_loop.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FuzzMessage, data, size, &run));
  run.Run();

  return 0;
}
