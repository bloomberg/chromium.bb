// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum.

// This file contains the list of sync ModelTypes that Android can select as
// preferred types.

DEFINE_MODEL_TYPE_SELECTION(AUTOFILL, 1)

DEFINE_MODEL_TYPE_SELECTION(BOOKMARK, 2)

DEFINE_MODEL_TYPE_SELECTION(PASSWORD, 4)

DEFINE_MODEL_TYPE_SELECTION(SESSION, 8)

DEFINE_MODEL_TYPE_SELECTION(TYPED_URL, 16)
