// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_INPUT_PREFERENCES_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_INPUT_PREFERENCES_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

class CONTENT_EXPORT SpeechInputPreferences
    : public base::RefCountedThreadSafe<SpeechInputPreferences> {
 public:
  SpeechInputPreferences();

  // Only to be called on the IO thread.
  virtual bool censor_results() const = 0;

  virtual void set_censor_results(bool censor_results) = 0;

 protected:
  virtual ~SpeechInputPreferences();

 private:
  friend class base::RefCountedThreadSafe<SpeechInputPreferences>;

  DISALLOW_COPY_AND_ASSIGN(SpeechInputPreferences);
};

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_INPUT_PREFERENCES_H_
