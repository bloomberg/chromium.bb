// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import android.view.Surface;

oneway interface ISandboxedProcessCallback {

  // Conduit to pass a Surface from the sandboxed renderer to the plugin.
  void establishSurfacePeer(
      int pid, int type, in Surface surface, int primaryID, int secondaryID);
}
