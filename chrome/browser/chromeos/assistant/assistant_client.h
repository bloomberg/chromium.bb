// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ASSISTANT_ASSISTANT_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_ASSISTANT_ASSISTANT_CLIENT_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/assistant/platform_audio_input_host.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {
namespace assistant {

// Class to handle all assistant in-browser-process functionalities.
class AssistantClient : mojom::Client {
 public:
  static AssistantClient* Get();

  AssistantClient();
  ~AssistantClient() override;

  void Start(service_manager::Connector* connector);

  // assistant::mojom::Client overrides:
  void OnAssistantStatusChanged(bool running) override;

 private:
  mojo::Binding<mojom::Client> client_binding_;

  mojom::AssistantPlatformPtr assistant_connection_;
  mojo::Binding<mojom::AudioInput> audio_input_binding_;

  PlatformAudioInputHost audio_input_;
  bool running_ = false;

  DISALLOW_COPY_AND_ASSIGN(AssistantClient);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ASSISTANT_ASSISTANT_CLIENT_H_
