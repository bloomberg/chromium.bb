/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* XRay -- a simple profiler for Native Client */


#ifndef LIBRARIES_XRAY_XRAY_H_
#define LIBRARIES_XRAY_XRAY_H_

#include <stdint.h>

#if defined(__arm__)
#undef XRAY
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define XRAY_NO_INSTRUMENT  __attribute__((no_instrument_function))
#define XRAY_INLINE __attribute__((always_inline))

#if defined(XRAY)

/* Do not call __XRayAnnotate* directly; instead use the */
/* XRayAnnotate() macros below. */
XRAY_NO_INSTRUMENT void __XRayAnnotate(const char* str, ...)
  __attribute__ ((format(printf, 1, 2)));
XRAY_NO_INSTRUMENT void __XRayAnnotateFiltered(const uint32_t filter,
  const char* str, ...) __attribute__ ((format(printf, 2, 3)));

/* This is the beginning of the public XRay API */
XRAY_NO_INSTRUMENT void XRayInit(int stack_size, int buffer_size,
                                 int frame_count, const char* mapfilename);
XRAY_NO_INSTRUMENT void XRayShutdown();
XRAY_NO_INSTRUMENT void XRayStartFrame();
XRAY_NO_INSTRUMENT void XRayEndFrame();
XRAY_NO_INSTRUMENT void XRaySetAnnotationFilter(uint32_t filter);
XRAY_NO_INSTRUMENT void XRaySaveReport(const char* filename, float cutoff);
#if defined(XRAY_ANNOTATE)
#define XRayAnnotate(...) __XRayAnnotate(__VA_ARGS__)
#define XRayAnnotateFiltered(...) __XRayAnnotateFiltered(__VA_ARGS__)
#else
#define XRayAnnotate(...)
#define XRayAnnotateFiltered(...)
#endif
/* This is the end of the public XRay API */

#else  /* defined(XRAY) */

/* Builds that don't define XRAY will use these 'null' functions instead. */

#define XRayAnnotate(...)
#define XRayAnnotateFiltered(...)

inline void XRayInit(int stack_size, int buffer_size,
                          int frame_count, const char* mapfilename) {}
inline void XRayShutdown() {}
inline void XRayStartFrame() {}
inline void XRayEndFrame() {}
inline void XRaySetAnnotationFilter(uint32_t filter) {}
inline void XRaySaveReport(const char* filename, float cutoff) {}

#endif  /* defined(XRAY) */

#ifdef __cplusplus
}
#endif

#endif  /* LIBRARIES_XRAY_XRAY_H_ */

