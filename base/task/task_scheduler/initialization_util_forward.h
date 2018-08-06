// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_INITIALIZATION_UTIL_FORWARD_H_
#define BASE_TASK_TASK_SCHEDULER_INITIALIZATION_UTIL_FORWARD_H_

// If you are seeing this you synced to a very short-lived state in the task
// scheduler's header migration. To keep as much git history as possible while
// avoiding a mass header rename in thousands of file in the main move CL,
// base/task_scheduler's public headers were moved this way :
// for each base/task_scheduler/public.h:
//  A.1) Introduce base/task/public_forward.h and move the world to use it
//  A.2) Move base/task_scheduler/public.h to base/task/public.h (and update
//       forwarding headers)
//  A.3) Delete base/task/public_forward.h (move world to base/task/public.h).
//
// An alternative would have been:
//  B.1) Move base/task_scheduler/public.h to base/task/public.h and leave
//       a stub fowarding header in base/task_scheduler/public.h
//  B.2) Migrate world from base/task_scheduler/public.h to base/task/public.h
//       and delete stub.
// That approach however results in git not considering base/task/public.h as
// being a new file rather than a move in (B.1) and losing all history.
//
// Another alternative would have been:
//  C.1) Introduce base/task/public.h as a forwarding header and move the world
//       to it
//  C.2) Move base/task_scheduler/public.h over base/task/public.h
// That approach also results in git not considering (C.2) a file move (per
// overriding an existing file) and losing all history.
//
// As such approach A was preferred : the landing process will wait for all 3
// CLs to be LGTM'ed in approach A and then land them switfly back-to-back.

#include "base/task/task_scheduler/initialization_util.h"

#endif  // BASE_TASK_TASK_SCHEDULER_INITIALIZATION_UTIL_FORWARD_H_
