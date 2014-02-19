// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_AUDIO_OBSERVER_H_
#define ASH_SYSTEM_AUDIO_AUDIO_OBSERVER_H_

namespace ash {

class AudioObserver {
 public:
  virtual ~AudioObserver() {}

  // Called when output volume changed.
  virtual void OnOutputVolumeChanged() = 0;

  // Called when output mute state changed.
  virtual void OnOutputMuteChanged() = 0;

  // Called when audio nodes changed.
  virtual void OnAudioNodesChanged() = 0;

  // Called when active audio output node changed.
  virtual void OnActiveOutputNodeChanged() = 0;

  // Called when active audio input node changed.
  virtual void OnActiveInputNodeChanged() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_AUDIO_OBSERVER_H_
