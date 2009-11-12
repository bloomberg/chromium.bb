// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LEAK_ANNOTATIONS_H_
#define BASE_LEAK_ANNOTATIONS_H_

#if defined(LINUX_USE_TCMALLOC)

#include "third_party/tcmalloc/heap-checker.h"

// Annotate a program scope as having memory leaks. Tcmalloc's heap leak
// checker will ignore them. Note that these annotations may mask real bugs
// and should not be used in the production code.
#define ANNOTATE_SCOPED_MEMORY_LEAK \
    HeapLeakChecker::Disabler heap_leak_checker_disabler

#else

// If tcmalloc is not used, the annotations should be no-ops.
#define ANNOTATE_SCOPED_MEMORY_LEAK

#endif

#endif  // BASE_LEAK_ANNOTATIONS_H_
