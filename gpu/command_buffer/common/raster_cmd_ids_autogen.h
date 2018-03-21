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
  OP(CompressedCopyTextureCHROMIUM)            /* 265 */ \
  OP(LoseContextCHROMIUM)                      /* 266 */ \
  OP(InsertFenceSyncCHROMIUM)                  /* 267 */ \
  OP(WaitSyncTokenCHROMIUM)                    /* 268 */ \
  OP(UnpremultiplyAndDitherCopyCHROMIUM)       /* 269 */ \
  OP(InitializeDiscardableTextureCHROMIUM)     /* 270 */ \
  OP(UnlockDiscardableTextureCHROMIUM)         /* 271 */ \
  OP(LockDiscardableTextureCHROMIUM)           /* 272 */ \
  OP(BeginRasterCHROMIUM)                      /* 273 */ \
  OP(RasterCHROMIUM)                           /* 274 */ \
  OP(EndRasterCHROMIUM)                        /* 275 */ \
  OP(CreateTransferCacheEntryINTERNAL)         /* 276 */ \
  OP(DeleteTransferCacheEntryINTERNAL)         /* 277 */ \
  OP(UnlockTransferCacheEntryINTERNAL)         /* 278 */ \
  OP(CreateTexture)                            /* 279 */ \
  OP(SetColorSpaceMetadata)                    /* 280 */ \
  OP(ProduceTextureDirectImmediate)            /* 281 */ \
  OP(CreateAndConsumeTextureINTERNALImmediate) /* 282 */ \
  OP(TexParameteri)                            /* 283 */ \
  OP(BindTexImage2DCHROMIUM)                   /* 284 */ \
  OP(ReleaseTexImage2DCHROMIUM)                /* 285 */ \
  OP(TexStorage2D)                             /* 286 */ \
  OP(CopySubTexture)                           /* 287 */

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
