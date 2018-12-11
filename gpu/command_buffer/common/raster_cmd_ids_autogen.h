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

#define RASTER_COMMAND_LIST(OP)                            \
  OP(DeleteTexturesImmediate)                    /* 256 */ \
  OP(Finish)                                     /* 257 */ \
  OP(Flush)                                      /* 258 */ \
  OP(GetError)                                   /* 259 */ \
  OP(GenQueriesEXTImmediate)                     /* 260 */ \
  OP(DeleteQueriesEXTImmediate)                  /* 261 */ \
  OP(BeginQueryEXT)                              /* 262 */ \
  OP(EndQueryEXT)                                /* 263 */ \
  OP(LoseContextCHROMIUM)                        /* 264 */ \
  OP(InsertFenceSyncCHROMIUM)                    /* 265 */ \
  OP(BeginRasterCHROMIUMImmediate)               /* 266 */ \
  OP(RasterCHROMIUM)                             /* 267 */ \
  OP(EndRasterCHROMIUM)                          /* 268 */ \
  OP(CreateTransferCacheEntryINTERNAL)           /* 269 */ \
  OP(DeleteTransferCacheEntryINTERNAL)           /* 270 */ \
  OP(UnlockTransferCacheEntryINTERNAL)           /* 271 */ \
  OP(DeletePaintCacheTextBlobsINTERNALImmediate) /* 272 */ \
  OP(DeletePaintCachePathsINTERNALImmediate)     /* 273 */ \
  OP(ClearPaintCacheINTERNAL)                    /* 274 */ \
  OP(CreateAndConsumeTextureINTERNALImmediate)   /* 275 */ \
  OP(CopySubTexture)                             /* 276 */ \
  OP(TraceBeginCHROMIUM)                         /* 277 */ \
  OP(TraceEndCHROMIUM)                           /* 278 */ \
  OP(SetActiveURLCHROMIUM)                       /* 279 */

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
