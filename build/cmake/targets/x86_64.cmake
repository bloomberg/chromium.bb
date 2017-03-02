##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
set(AOM_ARCH "x86_64")
set(ARCH_X86_64 1)

# Assembly flavors available for the target.
set(HAVE_MMX 1)
set(HAVE_SSE 1)
set(HAVE_SSE2 1)
set(HAVE_SSE3 1)
set(HAVE_SSSE3 1)
set(HAVE_SSE4_1 1)
set(HAVE_AVX 1)
set(HAVE_AVX2 1)

# RTCD versions of assembly flavor flags ("yes" means on in rtcd.pl, not 1).
set(RTCD_ARCH_X86_64 "yes")
set(RTCD_HAVE_MMX "yes")
set(RTCD_HAVE_SSE "yes")
set(RTCD_HAVE_SSE2 "yes")
set(RTCD_HAVE_SSE3 "yes")
set(RTCD_HAVE_SSSE3 "yes")
set(RTCD_HAVE_SSE4_1 "yes")
set(RTCD_HAVE_AVX "yes")
set(RTCD_HAVE_AVX2 "yes")

foreach (config_var ${AOM_CONFIG_VARS})
  if (${${config_var}})
    set(RTCD_${config_var} yes)
  endif ()
endforeach ()
