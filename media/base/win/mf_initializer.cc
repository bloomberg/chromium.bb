// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_initializer.h"

#include <mfapi.h>

#include "base/logging.h"

namespace media {

void InitializeMediaFoundation() {
  static HRESULT result = MFStartup(MF_VERSION, MFSTARTUP_LITE);
  DCHECK_EQ(result, S_OK);
}

}  // namespace media
