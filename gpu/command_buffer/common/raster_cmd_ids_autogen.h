// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_AUTOGEN_H_

#define RASTER_COMMAND_LIST(OP)                          \
  OP(DeleteTexturesImmediate)                  /* 256 */ \
  OP(Finish)                                   /* 257 */ \
  OP(Flush)                                    /* 258 */ \
  OP(GetError)                                 /* 259 */ \
  OP(GetIntegerv)                              /* 260 */ \
  OP(GenQueriesEXTImmediate)                   /* 261 */ \
  OP(DeleteQueriesEXTImmediate)                /* 262 */ \
  OP(BeginQueryEXT)                            /* 263 */ \
  OP(EndQueryEXT)                              /* 264 */ \
  OP(LoseContextCHROMIUM)                      /* 265 */ \
  OP(InsertFenceSyncCHROMIUM)                  /* 266 */ \
  OP(UnpremultiplyAndDitherCopyCHROMIUM)       /* 267 */ \
  OP(BeginRasterCHROMIUMImmediate)             /* 268 */ \
  OP(RasterCHROMIUM)                           /* 269 */ \
  OP(EndRasterCHROMIUM)                        /* 270 */ \
  OP(CreateTransferCacheEntryINTERNAL)         /* 271 */ \
  OP(DeleteTransferCacheEntryINTERNAL)         /* 272 */ \
  OP(UnlockTransferCacheEntryINTERNAL)         /* 273 */ \
  OP(CreateTexture)                            /* 274 */ \
  OP(SetColorSpaceMetadata)                    /* 275 */ \
  OP(ProduceTextureDirectImmediate)            /* 276 */ \
  OP(CreateAndConsumeTextureINTERNALImmediate) /* 277 */ \
  OP(TexParameteri)                            /* 278 */ \
  OP(BindTexImage2DCHROMIUM)                   /* 279 */ \
  OP(ReleaseTexImage2DCHROMIUM)                /* 280 */ \
  OP(TexStorage2D)                             /* 281 */ \
  OP(CopySubTexture)                           /* 282 */ \
  OP(TraceBeginCHROMIUM)                       /* 283 */ \
  OP(TraceEndCHROMIUM)                         /* 284 */ \
  OP(SetActiveURLCHROMIUM)                     /* 285 */ \
  OP(ResetActiveURLCHROMIUM)                   /* 286 */

enum CommandId {
  kOneBeforeStartPoint =
      cmd::kLastCommonId,  // All Raster commands start after this.
#define RASTER_CMD_OP(name) k##name,
  RASTER_COMMAND_LIST(RASTER_CMD_OP)
#undef RASTER_CMD_OP
      kNumCommands,
  kFirstRasterCommand = kOneBeforeStartPoint + 1
};

#endif  // GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_AUTOGEN_H_
