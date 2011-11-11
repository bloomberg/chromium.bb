// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_NOTIFICATION_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_NOTIFICATION_H_
#pragma once

#include "base/memory/scoped_ptr.h"

class Extension;
class Profile;
class SkBitmap;
class SkCanvas;
class StatusIcon;

// Manages the notification UI shown when an extension starts recording audio
// for speech recognition.
class SpeechInputExtensionNotification {
 public:
  explicit SpeechInputExtensionNotification(Profile* profile);
  virtual ~SpeechInputExtensionNotification();

  void Show(const Extension* extension, bool show_balloon);
  void Hide();
  void SetVUMeterVolume(float volume);

 private:
  void DrawVolume(SkCanvas* canvas, const SkBitmap& bitmap, float volume);
  void ShowNotificationBalloon(const Extension* extension);

  Profile* profile_;
  scoped_ptr<SkBitmap> mic_image_;
  scoped_ptr<SkBitmap> buffer_image_;
  StatusIcon* tray_icon_;
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_NOTIFICATION_H_
