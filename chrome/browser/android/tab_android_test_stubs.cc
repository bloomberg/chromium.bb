// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains stubs for some Chrome for Android specific code that is
// needed to compile some tests.

#include "printing/printing_context.h"
#include "printing/printing_context_android.h"


// static
printing::PrintingContext* printing::PrintingContext::Create(
    const std::string& app_locale) {
  return NULL;
}

// static
void printing::PrintingContextAndroid::PdfWritingDone(int fd, bool success) {
}

