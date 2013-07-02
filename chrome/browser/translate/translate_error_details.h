// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_ERROR_DETAILS_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_ERROR_DETAILS_H_

#include "base/time/time.h"
#include "chrome/common/translate/translate_errors.h"
#include "url/gurl.h"

struct TranslateErrorDetails {
  // The time when this was created
  base::Time time;

  // The URL
  GURL url;

  // Translation error type
  TranslateErrors::Type error;
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_ERROR_DETAILS_H_
