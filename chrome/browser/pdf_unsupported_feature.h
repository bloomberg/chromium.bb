// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_UNSUPPORTED_FEATURE_H_
#define CHROME_BROWSER_PDF_UNSUPPORTED_FEATURE_H_
#pragma once

#include "base/basictypes.h"

class TabContents;

// Call this when a tab encounters a PDF that has features which our internal
// viewer doesn't support.  Will take care of puting up an infobar to inform the
// user and launch Reader if they choose.  If Reader is out of date, it will put
// up an interstitial.
void PDFHasUnsupportedFeature(TabContents* tab);

#endif  // CHROME_BROWSER_PDF_UNSUPPORTED_FEATURE_H_
