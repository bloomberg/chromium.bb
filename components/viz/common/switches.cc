// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/switches.h"

namespace switches {

// Enables multi-client Surface synchronization. In practice, this indicates
// that LayerTreeHost expects to be given a valid viz::LocalSurfaceId provided
// by the parent compositor.
const char kEnableSurfaceSynchronization[] = "enable-surface-synchronization";

// Enables surface lifetime management using surface references. This disables
// adding surface sequences and instead adds temporary references. Surface
// lifetime is then based on temporary references and the surface reference
// graph.
const char kEnableSurfaceReferences[] = "enable-surface-references";

}  // namespace switches
