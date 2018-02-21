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

#define RASTER_COMMAND_LIST(OP)                      \
  OP(BindTexture)                          /* 256 */ \
  OP(DeleteTexturesImmediate)              /* 257 */ \
  OP(Finish)                               /* 258 */ \
  OP(Flush)                                /* 259 */ \
  OP(GenTexturesImmediate)                 /* 260 */ \
  OP(GetError)                             /* 261 */ \
  OP(GetIntegerv)                          /* 262 */ \
  OP(TexParameteri)                        /* 263 */ \
  OP(GenQueriesEXTImmediate)               /* 264 */ \
  OP(DeleteQueriesEXTImmediate)            /* 265 */ \
  OP(BeginQueryEXT)                        /* 266 */ \
  OP(EndQueryEXT)                          /* 267 */ \
  OP(CompressedCopyTextureCHROMIUM)        /* 268 */ \
  OP(LoseContextCHROMIUM)                  /* 269 */ \
  OP(InsertFenceSyncCHROMIUM)              /* 270 */ \
  OP(WaitSyncTokenCHROMIUM)                /* 271 */ \
  OP(UnpremultiplyAndDitherCopyCHROMIUM)   /* 272 */ \
  OP(InitializeDiscardableTextureCHROMIUM) /* 273 */ \
  OP(UnlockDiscardableTextureCHROMIUM)     /* 274 */ \
  OP(LockDiscardableTextureCHROMIUM)       /* 275 */ \
  OP(BeginRasterCHROMIUM)                  /* 276 */ \
  OP(RasterCHROMIUM)                       /* 277 */ \
  OP(EndRasterCHROMIUM)                    /* 278 */ \
  OP(CreateTransferCacheEntryINTERNAL)     /* 279 */ \
  OP(DeleteTransferCacheEntryINTERNAL)     /* 280 */ \
  OP(UnlockTransferCacheEntryINTERNAL)     /* 281 */

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
