/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef VPX_PORTS_SYSTEM_STATE_H_
#define VPX_PORTS_SYSTEM_STATE_H_

#include "./vpx_config.h"

#if ARCH_X86 || ARCH_X86_64
void vpx_reset_mmx_state(void);
#define vpx_clear_system_state() vpx_reset_mmx_state()
#else
#define vpx_clear_system_state()
#endif  // ARCH_X86 || ARCH_X86_64
#endif  // VPX_PORTS_SYSTEM_STATE_H_
