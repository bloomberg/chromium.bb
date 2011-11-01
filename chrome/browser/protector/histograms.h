// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_HISTOGRAMS_H_
#define CHROME_BROWSER_PROTECTOR_HISTOGRAMS_H_
#pragma once

namespace protector {

// Histogram name to report protection errors for the default search
// provider.
extern const char kProtectorHistogramDefaultSearchProvider[];

extern const char kProtectorBackupInvalidCounter[];
extern const char kProtectorValueChangedCounter[];
extern const char kProtectorValueValidCounter[];

// Protector histogram values.
enum ProtectorError {
  kProtectorErrorBackupInvalid,
  kProtectorErrorValueChanged,
  kProtectorErrorValueValid,

  // This is for convenience only, must always be the last.
  kProtectorErrorCount
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_HISTOGRAMS_H_

