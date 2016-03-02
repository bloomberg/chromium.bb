// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_COMMON_MUS_IPC_EXPORT_H_
#define COMPONENTS_MUS_COMMON_MUS_IPC_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(EVENTS_IPC_IMPLEMENTATION)
#define MUS_IPC_EXPORT __declspec(dllexport)
#else
#define MUS_IPC_EXPORT __declspec(dllimport)
#endif  // defined(EVENTS_IPC_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(EVENTS_IPC_IMPLEMENTATION)
#define MUS_IPC_EXPORT __attribute__((visibility("default")))
#else
#define MUS_IPC_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MUS_IPC_EXPORT
#endif

#endif  // COMPONENTS_MUS_COMMON_MUS_IPC_EXPORT_H_
