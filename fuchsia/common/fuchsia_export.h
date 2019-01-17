// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_COMMON_FUCHSIA_EXPORT_H_
#define FUCHSIA_COMMON_FUCHSIA_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WEBRUNNER_IMPLEMENTATION)
#define FUCHSIA_EXPORT __attribute__((visibility("default")))
#else
#define FUCHSIA_EXPORT
#endif

#else  // defined(COMPONENT_BUILD)
#define FUCHSIA_EXPORT
#endif

#endif  // FUCHSIA_COMMON_FUCHSIA_EXPORT_H_
