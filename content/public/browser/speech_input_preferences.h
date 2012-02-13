// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_INPUT_PREFERENCES_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_INPUT_PREFERENCES_H_
#pragma once

#include "base/memory/ref_counted.h"

namespace content {

class SpeechInputPreferences
    : public base::RefCountedThreadSafe<SpeechInputPreferences> {
 public:
  // Only to be called on the IO thread.
  virtual bool FilterProfanities() const = 0;

  virtual void SetFilterProfanities(bool filter_profanities) = 0;

 protected:
  virtual ~SpeechInputPreferences() {}

 private:
  friend class base::RefCountedThreadSafe<SpeechInputPreferences>;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_INPUT_PREFERENCES_H_
