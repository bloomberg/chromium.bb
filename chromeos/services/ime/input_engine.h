// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_INPUT_ENGINE_H_
#define CHROMEOS_SERVICES_IME_INPUT_ENGINE_H_

#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {
namespace ime {

// A basic implementation of InputEngine without using any decoder.
class InputEngine : public mojom::InputChannel {
 public:
  InputEngine();
  ~InputEngine() override;

  // Binds the mojom::InputChannel interface to this object.
  virtual void BindRequest(const std::string& ime_spec,
                           mojom::InputChannelRequest request,
                           mojom::InputChannelPtr client,
                           const std::vector<uint8_t>& extra);

  // mojom::MessageChannel overrides:
  void ProcessText(const std::string& message,
                   ProcessTextCallback callback) override;
  void ProcessMessage(const std::vector<uint8_t>& message,
                      ProcessMessageCallback callback) override;

  // TODO(https://crbug.com/837156): Implement a state for the interface.

 private:
  const std::string& Process(const std::string& message,
                             const std::string& ime_spec);

  mojo::BindingSet<mojom::InputChannel, std::string> channel_bindings_;

  DISALLOW_COPY_AND_ASSIGN(InputEngine);
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_INPUT_ENGINE_H_
