// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOCK_H_
#define BASE_LOCK_H_
#pragma once

// This is a temporary forwarding file so not every user of lock needs to
// be updated at once.
// TODO(brettw) remove this and fix everybody up to using the new location.
#include "base/synchronization/lock.h"

using base::AutoLock;
using base::AutoUnlock;
using base::Lock;

#endif  // BASE_LOCK_H_
