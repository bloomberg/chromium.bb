// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Pull in all the GUIDs we rely on.
#include "broker_lib.h"  // NOLINT
#include "chrome_tab.h"  // NOLINT
#include "toolband.h"  // NOLINT

#include <initguid.h>  // NOLINT

// Dispex guids - I don't know that they come in any library.
#include <dispex.h>

// Pull in the tab interface guids also.
#include "ceee/ie/common/ie_tab_interfaces.h"
#include "third_party/activscp/activdbg.h"

extern "C" {
#include "broker_lib_i.c"  // NOLINT
#include "toolband_i.c"  // NOLINT

const GUID IID_IProcessDebugManager = __uuidof(IProcessDebugManager);
const GUID IID_IDebugApplication = __uuidof(IDebugApplication);
const GUID IID_IDebugDocumentHelper = __uuidof(IDebugDocumentHelper);
}  // extern "C"
