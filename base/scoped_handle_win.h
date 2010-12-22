// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(brettw) remove this file when all callers are converted to using the
// new location/namespace
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_hglobal.h"

using base::win::ScopedBitmap;
using base::win::ScopedHandle;
using base::win::ScopedHDC;
using base::win::ScopedHFONT;
using base::win::ScopedHGlobal;
using base::win::ScopedHICON;
using base::win::ScopedRegion;
