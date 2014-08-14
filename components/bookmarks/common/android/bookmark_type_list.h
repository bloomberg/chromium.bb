// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values.

#ifndef DEFINE_BOOKMARK_TYPE
#error "DEFINE_BOOKMARK_TYPE should be defined before including this file"
#endif

DEFINE_BOOKMARK_TYPE(NORMAL, 0)
DEFINE_BOOKMARK_TYPE(PARTNER, 1)
