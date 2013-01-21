// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_TRAY_ICON_CONTROLLER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_TRAY_ICON_CONTROLLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"

class Extension;
class SkBitmap;
class SkCanvas;
class StatusIcon;

namespace gfx {
class ImageSkia;
}

// Manages the tray icon for speech recognition.
class SpeechRecognitionTrayIconController
     : public base::RefCountedThreadSafe<SpeechRecognitionTrayIconController> {
 public:
  SpeechRecognitionTrayIconController();

  void Show(const string16& tooltip);
  void Hide();
  void SetVUMeterVolume(float volume);

 private:
  friend class base::RefCountedThreadSafe<SpeechRecognitionTrayIconController>;
  virtual ~SpeechRecognitionTrayIconController();

  void Initialize();
  void DrawVolume(SkCanvas* canvas, const gfx::ImageSkia& image, float volume);

  scoped_ptr<SkBitmap> mic_image_;
  scoped_ptr<SkBitmap> buffer_image_;
  StatusIcon* tray_icon_;
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_TRAY_ICON_CONTROLLER_H_
