// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webkit_glue.h"

namespace webkit_glue {

string16 GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

}  // namespace webkit_glue
