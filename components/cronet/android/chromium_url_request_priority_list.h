// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values.
#ifndef DEFINE_REQUEST_PRIORITY
#error "DEFINE_REQUEST_PRIORITY should be defined before including this file"
#endif
DEFINE_REQUEST_PRIORITY(IDLE, 0)
DEFINE_REQUEST_PRIORITY(LOWEST, 1)
DEFINE_REQUEST_PRIORITY(LOW, 2)
DEFINE_REQUEST_PRIORITY(MEDIUM, 3)
DEFINE_REQUEST_PRIORITY(HIGHEST, 4)
