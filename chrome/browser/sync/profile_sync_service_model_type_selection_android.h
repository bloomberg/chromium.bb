// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values.

// This file contains the list of sync ModelTypes that Android can register for
// invalidations for.

DEFINE_MODEL_TYPE_SELECTION(AUTOFILL, 1<<0)

DEFINE_MODEL_TYPE_SELECTION(BOOKMARK, 1<<1)

DEFINE_MODEL_TYPE_SELECTION(PASSWORD, 1<<2)

DEFINE_MODEL_TYPE_SELECTION(SESSION, 1<<3)

DEFINE_MODEL_TYPE_SELECTION(TYPED_URL, 1<<4)

DEFINE_MODEL_TYPE_SELECTION(AUTOFILL_PROFILE, 1<<5)

DEFINE_MODEL_TYPE_SELECTION(HISTORY_DELETE_DIRECTIVE, 1<<6)

DEFINE_MODEL_TYPE_SELECTION(PROXY_TABS, 1<<7)

DEFINE_MODEL_TYPE_SELECTION(FAVICON_IMAGE, 1<<8)

DEFINE_MODEL_TYPE_SELECTION(FAVICON_TRACKING, 1<<9)

DEFINE_MODEL_TYPE_SELECTION(NIGORI, 1<<10)

DEFINE_MODEL_TYPE_SELECTION(DEVICE_INFO, 1<<11)

DEFINE_MODEL_TYPE_SELECTION(EXPERIMENTS, 1<<12)
