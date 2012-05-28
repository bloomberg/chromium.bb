// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_EMAIL_DETECTOR_H_
#define CONTENT_RENDERER_ANDROID_EMAIL_DETECTOR_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/renderer/android/content_detector.h"

class EmailDetectorTest;

namespace content {

// Finds email addresses (in most common formats, but not including special
// characters) in the given text string.
class EmailDetector : public ContentDetector {
 public:
  EmailDetector();

 private:
  friend class ::EmailDetectorTest;

  // Implementation of ContentDetector.
  virtual bool FindContent(const string16::const_iterator& begin,
                           const string16::const_iterator& end,
                           size_t* start_pos,
                           size_t* end_pos,
                           std::string* content_text) OVERRIDE;
  virtual GURL GetIntentURL(const std::string& content_text) OVERRIDE;
  virtual size_t GetMaximumContentLength() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(EmailDetector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_EMAIL_DETECTOR_H_
