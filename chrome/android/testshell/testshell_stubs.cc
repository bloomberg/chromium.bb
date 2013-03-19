// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"

// This file contains temporary stubs to allow the libtestshell target to
// compile. They will be removed once real implementations are
// written/upstreamed, or once other code is refactored to eliminate the
// need for them.

// static
TabAndroid* TabAndroid::FromWebContents(content::WebContents* web_contents) {
  return NULL;
}

