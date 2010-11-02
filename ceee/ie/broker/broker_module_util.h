// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Utility functions to be called on the Broker Module object.

#ifndef CEEE_IE_BROKER_BROKER_MODULE_UTIL_H__
#define CEEE_IE_BROKER_BROKER_MODULE_UTIL_H__

namespace ceee_module_util {

// Locks the module to prevent it from being terminated.
//
// @return The new module ref count, but only for debugging purposes.
LONG LockModule();

// Unlocks the module to allow it to be terminated appropriately.
//
// @return The new module ref count, but only for debugging purposes.
LONG UnlockModule();

}  // namespace

#endif  // CEEE_IE_BROKER_BROKER_MODULE_UTIL_H__
