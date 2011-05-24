// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines helper functions shared by the various implementations
// of OmniboxView.

#include "chrome/browser/ui/omnibox/omnibox_view.h"

#include "base/string_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"

string16 OmniboxView::StripJavascriptSchemas(const string16& text) {
  const string16 kJsPrefix(ASCIIToUTF16(chrome::kJavaScriptScheme) +
                           ASCIIToUTF16(":"));
  string16 out(text);
  while (StartsWith(out, kJsPrefix, false))
    TrimWhitespace(out.substr(kJsPrefix.length()), TRIM_LEADING, &out);
  return out;
}

