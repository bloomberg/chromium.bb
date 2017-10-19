// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/switches.h"

namespace switches {

// Disable surface lifetime management using surface references. This enables
// adding surface sequences and disables adding temporary references.
const char kDisableSurfaceReferences[] = "disable-surface-references";

// Enables multi-client Surface synchronization. In practice, this indicates
// that LayerTreeHost expects to be given a valid viz::LocalSurfaceId provided
// by the parent compositor.
const char kEnableSurfaceSynchronization[] = "enable-surface-synchronization";

// Enables running viz. This basically entails running the display compositor
// in the viz process instead of the browser process.
const char kEnableViz[] = "enable-viz";

}  // namespace switches
