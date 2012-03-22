/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Untrusted crash dumper.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_CRASH_DUMP_UNTRUSTED_CRASH_DUMP_H__
#define NATIVE_CLIENT_SRC_UNTRUSTED_CRASH_DUMP_UNTRUSTED_CRASH_DUMP_H__ 1


#ifdef __cplusplus
extern "C" {
#endif

/* These return non-zero on success. */
int NaClCrashDumpInit(void);
int NaClCrashDumpInitThread(void);

#ifdef __cplusplus
}
#endif

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_CRASH_DUMP_UNTRUSTED_CRASH_DUMP_H__ */
