// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards because this file
// is meant to be included inside a macro to generate enum values.

// This file contains a list of events relating to selection and insertion, used
// for notifying Java when the renderer selection has changed.

#ifndef DEFINE_SELECTION_EVENT_TYPE
#error "Please define DEFINE_SELECTION_EVENT_TYPE before including this file."
#endif

DEFINE_SELECTION_EVENT_TYPE(SELECTION_SHOWN, 0)
DEFINE_SELECTION_EVENT_TYPE(SELECTION_CLEARED, 1)
DEFINE_SELECTION_EVENT_TYPE(INSERTION_SHOWN, 2)
DEFINE_SELECTION_EVENT_TYPE(INSERTION_MOVED, 3)
DEFINE_SELECTION_EVENT_TYPE(INSERTION_TAPPED, 4)
DEFINE_SELECTION_EVENT_TYPE(INSERTION_CLEARED, 5)
