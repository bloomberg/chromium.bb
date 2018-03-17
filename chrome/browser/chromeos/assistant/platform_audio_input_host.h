// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_H_
#define CHROME_BROWSER_CHROMEOS_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace media {
class AudioInputController;
}  // namespace media

namespace chromeos {
namespace assistant {

// Interacts with AudioController and forwards audio input stream to assistant.
class PlatformAudioInputHost : public mojom::AudioInput {
 public:
  PlatformAudioInputHost();
  ~PlatformAudioInputHost() override;

  // mojom::AudioInput overrides:
  void AddObserver(mojom::AudioInputObserverPtr observer) override;

  void NotifyDataAvailable(const std::vector<int32_t>& data,
                           int32_t frames,
                           base::TimeTicks capture_time);
  void NotifyAudioClosed();

 private:
  class Writer;
  class EventHandler;

  scoped_refptr<media::AudioInputController> audio_input_controller_;
  std::unique_ptr<Writer> sync_writer_;
  std::unique_ptr<EventHandler> event_handler_;
  mojo::InterfacePtrSet<mojom::AudioInputObserver> observers_;

  bool recording_ = false;

  base::WeakPtrFactory<PlatformAudioInputHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAudioInputHost);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_H_
