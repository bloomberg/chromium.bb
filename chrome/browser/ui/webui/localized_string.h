// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOCALIZED_STRING_H_
#define CHROME_BROWSER_UI_WEBUI_LOCALIZED_STRING_H_

#include <stddef.h>

#include "base/containers/span.h"

namespace content {
class WebUIDataSource;
}

struct LocalizedString {
  const char* name;
  int id;
};

// Calls content::WebUIDataSource::AddLocalizedString() in a for-loop for
// |strings|. Reduces code size vs. reimplementing the same for-loop.
void AddLocalizedStringsBulk(content::WebUIDataSource* html_source,
                             base::span<const LocalizedString> strings);

#endif  // CHROME_BROWSER_UI_WEBUI_LOCALIZED_STRING_H_
