// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_PREFERENCES_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_PREFERENCES_H_
#pragma once

#include "base/memory/ref_counted.h"

namespace content {

class SpeechRecognitionPreferences
    : public base::RefCountedThreadSafe<SpeechRecognitionPreferences> {
 public:
  virtual bool FilterProfanities() const = 0;

 protected:
  virtual ~SpeechRecognitionPreferences() {}

 private:
  friend class base::RefCountedThreadSafe<SpeechRecognitionPreferences>;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_PREFERENCES_H_
