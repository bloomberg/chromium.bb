// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SCOPED_NSOBJECT_H_
#define BASE_MEMORY_SCOPED_NSOBJECT_H_

// TODO(thakis): Update all clients to include base/mac/scoped_nsobject.h and
// get rid of this forwarding header.
#include "base/mac/scoped_nsobject.h"

using base::scoped_nsobject;
using base::scoped_nsprotocol;

#endif  // BASE_MEMORY_SCOPED_NSOBJECT_H_
