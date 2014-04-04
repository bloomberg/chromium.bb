// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values.
#ifndef DEFINE_REQUEST_ERROR
#error "DEFINE_REQUEST_ERROR should be defined before including this file"
#endif
DEFINE_REQUEST_ERROR(SUCCESS, 0)
DEFINE_REQUEST_ERROR(UNKNOWN, 1)
DEFINE_REQUEST_ERROR(MALFORMED_URL, 2)
DEFINE_REQUEST_ERROR(CONNECTION_TIMED_OUT, 3)
DEFINE_REQUEST_ERROR(UNKNOWN_HOST, 4)
