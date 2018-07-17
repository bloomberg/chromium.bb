// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"

#include "chromeos/services/ime/public/mojom/constants.mojom.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"

#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"

namespace chromeos {
namespace ime {

namespace {

const char kTestServiceName[] = "ime_unittests";
const std::vector<uint8_t> extra{0x66, 0x77, 0x88};

void RunCallback(bool* success, bool result) {
  *success = result;
}

class ImeServiceTest : public service_manager::test::ServiceTest {
 public:
  ImeServiceTest() : service_manager::test::ServiceTest(kTestServiceName) {}
  ~ImeServiceTest() override {}

  void SetUp() override {
    ServiceTest::SetUp();
    // TODO(https://crbug.com/837156): Start or bind other services used.
    // Eg.  connector()->StartService(kServiceName) or
    //      connector()->BindInterface(kServiceName, &ptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImeServiceTest);
};

class FakeClient : mojom::InputChannel {
 public:
  FakeClient() : binding_(this) {}

  mojom::InputChannelPtr CreateInterfacePtrAndBind() {
    mojom::InputChannelPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

 private:
  // mojom::InputEngineClient:
  void ProcessText(const std::string& message,
                   ProcessTextCallback callback) override{};
  void ProcessMessage(const std::vector<uint8_t>& message,
                      ProcessMessageCallback callback) override{};

  mojo::Binding<mojom::InputChannel> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeClient);
};

}  // namespace

// Tests activating an IME and set its delegate.
TEST_F(ImeServiceTest, ConnectImeEngine) {
  connector()->StartService(mojom::kServiceName);

  mojom::InputEngineManagerPtr manager;
  connector()->BindInterface(mojom::kServiceName, &manager);

  bool success = false;
  FakeClient client;

  mojom::InputChannelPtr to_engine_ptr;

  manager->ConnectToImeEngine("FakeIME", mojo::MakeRequest(&to_engine_ptr),
                              client.CreateInterfacePtrAndBind(), extra,
                              base::BindOnce(&RunCallback, &success));
}

}  // namespace ime
}  // namespace chromeos
