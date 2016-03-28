##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##


AV1_CX_EXPORTS += exports_enc

AV1_CX_SRCS-yes += $(AV1_COMMON_SRCS-yes)
AV1_CX_SRCS-no  += $(AV1_COMMON_SRCS-no)
AV1_CX_SRCS_REMOVE-yes += $(AV1_COMMON_SRCS_REMOVE-yes)
AV1_CX_SRCS_REMOVE-no  += $(AV1_COMMON_SRCS_REMOVE-no)

AV1_CX_SRCS-yes += av1_cx_iface.c

AV1_CX_SRCS-yes += encoder/bitstream.c
AV1_CX_SRCS-yes += encoder/context_tree.c
AV1_CX_SRCS-yes += encoder/context_tree.h
AV1_CX_SRCS-yes += encoder/cost.h
AV1_CX_SRCS-yes += encoder/cost.c
AV1_CX_SRCS-yes += encoder/dct.c
AV1_CX_SRCS-yes += encoder/encodeframe.c
AV1_CX_SRCS-yes += encoder/encodeframe.h
AV1_CX_SRCS-yes += encoder/encodemb.c
AV1_CX_SRCS-yes += encoder/encodemv.c
AV1_CX_SRCS-yes += encoder/ethread.h
AV1_CX_SRCS-yes += encoder/ethread.c
AV1_CX_SRCS-yes += encoder/extend.c
AV1_CX_SRCS-yes += encoder/firstpass.c
AV1_CX_SRCS-yes += encoder/block.h
AV1_CX_SRCS-yes += encoder/bitstream.h
AV1_CX_SRCS-yes += encoder/encodemb.h
AV1_CX_SRCS-yes += encoder/encodemv.h
AV1_CX_SRCS-yes += encoder/extend.h
AV1_CX_SRCS-yes += encoder/firstpass.h
AV1_CX_SRCS-yes += encoder/lookahead.c
AV1_CX_SRCS-yes += encoder/lookahead.h
AV1_CX_SRCS-yes += encoder/mcomp.h
AV1_CX_SRCS-yes += encoder/encoder.h
AV1_CX_SRCS-yes += encoder/quantize.h
AV1_CX_SRCS-yes += encoder/ratectrl.h
AV1_CX_SRCS-yes += encoder/rd.h
AV1_CX_SRCS-yes += encoder/rdopt.h
AV1_CX_SRCS-yes += encoder/tokenize.h
AV1_CX_SRCS-yes += encoder/treewriter.h
AV1_CX_SRCS-yes += encoder/mcomp.c
AV1_CX_SRCS-yes += encoder/encoder.c
AV1_CX_SRCS-yes += encoder/picklpf.c
AV1_CX_SRCS-yes += encoder/picklpf.h
AV1_CX_SRCS-yes += encoder/quantize.c
AV1_CX_SRCS-yes += encoder/ratectrl.c
AV1_CX_SRCS-yes += encoder/rd.c
AV1_CX_SRCS-yes += encoder/rdopt.c
AV1_CX_SRCS-yes += encoder/segmentation.c
AV1_CX_SRCS-yes += encoder/segmentation.h
AV1_CX_SRCS-yes += encoder/speed_features.c
AV1_CX_SRCS-yes += encoder/speed_features.h
AV1_CX_SRCS-yes += encoder/subexp.c
AV1_CX_SRCS-yes += encoder/subexp.h
AV1_CX_SRCS-yes += encoder/resize.c
AV1_CX_SRCS-yes += encoder/resize.h
AV1_CX_SRCS-$(CONFIG_INTERNAL_STATS) += encoder/blockiness.c

AV1_CX_SRCS-yes += encoder/tokenize.c
AV1_CX_SRCS-yes += encoder/treewriter.c
AV1_CX_SRCS-yes += encoder/aq_variance.c
AV1_CX_SRCS-yes += encoder/aq_variance.h
AV1_CX_SRCS-yes += encoder/aq_cyclicrefresh.c
AV1_CX_SRCS-yes += encoder/aq_cyclicrefresh.h
AV1_CX_SRCS-yes += encoder/aq_complexity.c
AV1_CX_SRCS-yes += encoder/aq_complexity.h
AV1_CX_SRCS-yes += encoder/skin_detection.c
AV1_CX_SRCS-yes += encoder/skin_detection.h
AV1_CX_SRCS-yes += encoder/temporal_filter.c
AV1_CX_SRCS-yes += encoder/temporal_filter.h
AV1_CX_SRCS-yes += encoder/mbgraph.c
AV1_CX_SRCS-yes += encoder/mbgraph.h
AV1_CX_SRCS-yes += encoder/pickdering.c

AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/temporal_filter_apply_sse2.asm
AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/quantize_sse2.c
ifeq ($(CONFIG_AOM_HIGHBITDEPTH),yes)
AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/highbd_block_error_intrin_sse2.c
endif

ifeq ($(CONFIG_USE_X86INC),yes)
AV1_CX_SRCS-$(HAVE_MMX) += encoder/x86/dct_mmx.asm
AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/error_sse2.asm
endif

ifeq ($(ARCH_X86_64),yes)
ifeq ($(CONFIG_USE_X86INC),yes)
AV1_CX_SRCS-$(HAVE_SSSE3) += encoder/x86/quantize_ssse3_x86_64.asm
endif
endif

AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/dct_sse2.c
AV1_CX_SRCS-$(HAVE_SSSE3) += encoder/x86/dct_ssse3.c

AV1_CX_SRCS-$(HAVE_AVX2) += encoder/x86/error_intrin_avx2.c

ifneq ($(CONFIG_AOM_HIGHBITDEPTH),yes)
AV1_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/dct_neon.c
AV1_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/error_neon.c
endif
AV1_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/quantize_neon.c

AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/error_msa.c
AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct4x4_msa.c
AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct8x8_msa.c
AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct16x16_msa.c
AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct_msa.h
AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/temporal_filter_msa.c

AV1_CX_SRCS-yes := $(filter-out $(AV1_CX_SRCS_REMOVE-yes),$(AV1_CX_SRCS-yes))
