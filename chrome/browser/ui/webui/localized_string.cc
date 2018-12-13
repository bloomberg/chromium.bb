// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/localized_string.h"

#include "content/public/browser/web_ui_data_source.h"

void AddLocalizedStringsBulk(content::WebUIDataSource* html_source,
                             const LocalizedString* strings,
                             size_t num_strings) {
  for (size_t i = 0; i < num_strings; ++i)
    html_source->AddLocalizedString(strings[i].name, strings[i].id);
}
