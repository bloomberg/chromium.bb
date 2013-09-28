// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "printing/printing_context.h"
#include "printing/printing_context_android.h"

// This file contains temporary stubs to allow the libtestshell target to
// compile. They will be removed once real implementations are
// written/upstreamed, or once other code is refactored to eliminate the
// need for them.

// static
printing::PrintingContext* printing::PrintingContext::Create(
    const std::string& app_locale) {
  return NULL;
}

// static
void printing::PrintingContextAndroid::PdfWritingDone(int fd, bool success) {
}

