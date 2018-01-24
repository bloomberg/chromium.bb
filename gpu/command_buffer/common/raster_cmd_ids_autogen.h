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

#define RASTER_COMMAND_LIST(OP)                       \
  OP(ActiveTexture)                         /* 256 */ \
  OP(BindTexture)                           /* 257 */ \
  OP(CompressedTexImage2DBucket)            /* 258 */ \
  OP(CompressedTexImage2D)                  /* 259 */ \
  OP(DeleteTexturesImmediate)               /* 260 */ \
  OP(Finish)                                /* 261 */ \
  OP(Flush)                                 /* 262 */ \
  OP(GenTexturesImmediate)                  /* 263 */ \
  OP(GetError)                              /* 264 */ \
  OP(GetIntegerv)                           /* 265 */ \
  OP(PixelStorei)                           /* 266 */ \
  OP(TexImage2D)                            /* 267 */ \
  OP(TexParameteri)                         /* 268 */ \
  OP(TexSubImage2D)                         /* 269 */ \
  OP(TexStorage2DEXT)                       /* 270 */ \
  OP(GenQueriesEXTImmediate)                /* 271 */ \
  OP(DeleteQueriesEXTImmediate)             /* 272 */ \
  OP(BeginQueryEXT)                         /* 273 */ \
  OP(EndQueryEXT)                           /* 274 */ \
  OP(CopySubTextureCHROMIUM)                /* 275 */ \
  OP(CompressedCopyTextureCHROMIUM)         /* 276 */ \
  OP(ProduceTextureDirectCHROMIUMImmediate) /* 277 */ \
  OP(BindTexImage2DCHROMIUM)                /* 278 */ \
  OP(ReleaseTexImage2DCHROMIUM)             /* 279 */ \
  OP(TraceBeginCHROMIUM)                    /* 280 */ \
  OP(TraceEndCHROMIUM)                      /* 281 */ \
  OP(LoseContextCHROMIUM)                   /* 282 */ \
  OP(WaitSyncTokenCHROMIUM)                 /* 283 */ \
  OP(InitializeDiscardableTextureCHROMIUM)  /* 284 */ \
  OP(UnlockDiscardableTextureCHROMIUM)      /* 285 */ \
  OP(LockDiscardableTextureCHROMIUM)        /* 286 */ \
  OP(BeginRasterCHROMIUM)                   /* 287 */ \
  OP(RasterCHROMIUM)                        /* 288 */ \
  OP(EndRasterCHROMIUM)                     /* 289 */ \
  OP(CreateTransferCacheEntryINTERNAL)      /* 290 */ \
  OP(DeleteTransferCacheEntryINTERNAL)      /* 291 */ \
  OP(UnlockTransferCacheEntryINTERNAL)      /* 292 */ \
  OP(TexStorage2DImageCHROMIUM)             /* 293 */

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
