// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_IDS_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_IDS_AUTOGEN_H_

#define GLES2_COMMAND_LIST(OP)                           \
  OP(ActiveTexture)                            /* 256 */ \
  OP(AttachShader)                             /* 257 */ \
  OP(BindAttribLocationBucket)                 /* 258 */ \
  OP(BindBuffer)                               /* 259 */ \
  OP(BindFramebuffer)                          /* 260 */ \
  OP(BindRenderbuffer)                         /* 261 */ \
  OP(BindTexture)                              /* 262 */ \
  OP(BlendColor)                               /* 263 */ \
  OP(BlendEquation)                            /* 264 */ \
  OP(BlendEquationSeparate)                    /* 265 */ \
  OP(BlendFunc)                                /* 266 */ \
  OP(BlendFuncSeparate)                        /* 267 */ \
  OP(BufferData)                               /* 268 */ \
  OP(BufferSubData)                            /* 269 */ \
  OP(CheckFramebufferStatus)                   /* 270 */ \
  OP(Clear)                                    /* 271 */ \
  OP(ClearColor)                               /* 272 */ \
  OP(ClearDepthf)                              /* 273 */ \
  OP(ClearStencil)                             /* 274 */ \
  OP(ColorMask)                                /* 275 */ \
  OP(CompileShader)                            /* 276 */ \
  OP(CompressedTexImage2DBucket)               /* 277 */ \
  OP(CompressedTexImage2D)                     /* 278 */ \
  OP(CompressedTexSubImage2DBucket)            /* 279 */ \
  OP(CompressedTexSubImage2D)                  /* 280 */ \
  OP(CopyBufferSubData)                        /* 281 */ \
  OP(CopyTexImage2D)                           /* 282 */ \
  OP(CopyTexSubImage2D)                        /* 283 */ \
  OP(CreateProgram)                            /* 284 */ \
  OP(CreateShader)                             /* 285 */ \
  OP(CullFace)                                 /* 286 */ \
  OP(DeleteBuffersImmediate)                   /* 287 */ \
  OP(DeleteFramebuffersImmediate)              /* 288 */ \
  OP(DeleteProgram)                            /* 289 */ \
  OP(DeleteRenderbuffersImmediate)             /* 290 */ \
  OP(DeleteShader)                             /* 291 */ \
  OP(DeleteTexturesImmediate)                  /* 292 */ \
  OP(DepthFunc)                                /* 293 */ \
  OP(DepthMask)                                /* 294 */ \
  OP(DepthRangef)                              /* 295 */ \
  OP(DetachShader)                             /* 296 */ \
  OP(Disable)                                  /* 297 */ \
  OP(DisableVertexAttribArray)                 /* 298 */ \
  OP(DrawArrays)                               /* 299 */ \
  OP(DrawElements)                             /* 300 */ \
  OP(Enable)                                   /* 301 */ \
  OP(EnableVertexAttribArray)                  /* 302 */ \
  OP(Finish)                                   /* 303 */ \
  OP(Flush)                                    /* 304 */ \
  OP(FramebufferRenderbuffer)                  /* 305 */ \
  OP(FramebufferTexture2D)                     /* 306 */ \
  OP(FramebufferTextureLayer)                  /* 307 */ \
  OP(FrontFace)                                /* 308 */ \
  OP(GenBuffersImmediate)                      /* 309 */ \
  OP(GenerateMipmap)                           /* 310 */ \
  OP(GenFramebuffersImmediate)                 /* 311 */ \
  OP(GenRenderbuffersImmediate)                /* 312 */ \
  OP(GenTexturesImmediate)                     /* 313 */ \
  OP(GetActiveAttrib)                          /* 314 */ \
  OP(GetActiveUniform)                         /* 315 */ \
  OP(GetAttachedShaders)                       /* 316 */ \
  OP(GetAttribLocation)                        /* 317 */ \
  OP(GetBooleanv)                              /* 318 */ \
  OP(GetBufferParameteriv)                     /* 319 */ \
  OP(GetError)                                 /* 320 */ \
  OP(GetFloatv)                                /* 321 */ \
  OP(GetFramebufferAttachmentParameteriv)      /* 322 */ \
  OP(GetIntegerv)                              /* 323 */ \
  OP(GetInternalformativ)                      /* 324 */ \
  OP(GetProgramiv)                             /* 325 */ \
  OP(GetProgramInfoLog)                        /* 326 */ \
  OP(GetRenderbufferParameteriv)               /* 327 */ \
  OP(GetShaderiv)                              /* 328 */ \
  OP(GetShaderInfoLog)                         /* 329 */ \
  OP(GetShaderPrecisionFormat)                 /* 330 */ \
  OP(GetShaderSource)                          /* 331 */ \
  OP(GetString)                                /* 332 */ \
  OP(GetTexParameterfv)                        /* 333 */ \
  OP(GetTexParameteriv)                        /* 334 */ \
  OP(GetUniformfv)                             /* 335 */ \
  OP(GetUniformiv)                             /* 336 */ \
  OP(GetUniformLocation)                       /* 337 */ \
  OP(GetVertexAttribfv)                        /* 338 */ \
  OP(GetVertexAttribiv)                        /* 339 */ \
  OP(GetVertexAttribPointerv)                  /* 340 */ \
  OP(Hint)                                     /* 341 */ \
  OP(IsBuffer)                                 /* 342 */ \
  OP(IsEnabled)                                /* 343 */ \
  OP(IsFramebuffer)                            /* 344 */ \
  OP(IsProgram)                                /* 345 */ \
  OP(IsRenderbuffer)                           /* 346 */ \
  OP(IsShader)                                 /* 347 */ \
  OP(IsTexture)                                /* 348 */ \
  OP(LineWidth)                                /* 349 */ \
  OP(LinkProgram)                              /* 350 */ \
  OP(PixelStorei)                              /* 351 */ \
  OP(PolygonOffset)                            /* 352 */ \
  OP(ReadPixels)                               /* 353 */ \
  OP(ReleaseShaderCompiler)                    /* 354 */ \
  OP(RenderbufferStorage)                      /* 355 */ \
  OP(SampleCoverage)                           /* 356 */ \
  OP(Scissor)                                  /* 357 */ \
  OP(ShaderBinary)                             /* 358 */ \
  OP(ShaderSourceBucket)                       /* 359 */ \
  OP(StencilFunc)                              /* 360 */ \
  OP(StencilFuncSeparate)                      /* 361 */ \
  OP(StencilMask)                              /* 362 */ \
  OP(StencilMaskSeparate)                      /* 363 */ \
  OP(StencilOp)                                /* 364 */ \
  OP(StencilOpSeparate)                        /* 365 */ \
  OP(TexImage2D)                               /* 366 */ \
  OP(TexParameterf)                            /* 367 */ \
  OP(TexParameterfvImmediate)                  /* 368 */ \
  OP(TexParameteri)                            /* 369 */ \
  OP(TexParameterivImmediate)                  /* 370 */ \
  OP(TexSubImage2D)                            /* 371 */ \
  OP(Uniform1f)                                /* 372 */ \
  OP(Uniform1fvImmediate)                      /* 373 */ \
  OP(Uniform1i)                                /* 374 */ \
  OP(Uniform1ivImmediate)                      /* 375 */ \
  OP(Uniform2f)                                /* 376 */ \
  OP(Uniform2fvImmediate)                      /* 377 */ \
  OP(Uniform2i)                                /* 378 */ \
  OP(Uniform2ivImmediate)                      /* 379 */ \
  OP(Uniform3f)                                /* 380 */ \
  OP(Uniform3fvImmediate)                      /* 381 */ \
  OP(Uniform3i)                                /* 382 */ \
  OP(Uniform3ivImmediate)                      /* 383 */ \
  OP(Uniform4f)                                /* 384 */ \
  OP(Uniform4fvImmediate)                      /* 385 */ \
  OP(Uniform4i)                                /* 386 */ \
  OP(Uniform4ivImmediate)                      /* 387 */ \
  OP(UniformMatrix2fvImmediate)                /* 388 */ \
  OP(UniformMatrix3fvImmediate)                /* 389 */ \
  OP(UniformMatrix4fvImmediate)                /* 390 */ \
  OP(UseProgram)                               /* 391 */ \
  OP(ValidateProgram)                          /* 392 */ \
  OP(VertexAttrib1f)                           /* 393 */ \
  OP(VertexAttrib1fvImmediate)                 /* 394 */ \
  OP(VertexAttrib2f)                           /* 395 */ \
  OP(VertexAttrib2fvImmediate)                 /* 396 */ \
  OP(VertexAttrib3f)                           /* 397 */ \
  OP(VertexAttrib3fvImmediate)                 /* 398 */ \
  OP(VertexAttrib4f)                           /* 399 */ \
  OP(VertexAttrib4fvImmediate)                 /* 400 */ \
  OP(VertexAttribPointer)                      /* 401 */ \
  OP(Viewport)                                 /* 402 */ \
  OP(BlitFramebufferCHROMIUM)                  /* 403 */ \
  OP(RenderbufferStorageMultisampleCHROMIUM)   /* 404 */ \
  OP(RenderbufferStorageMultisampleEXT)        /* 405 */ \
  OP(FramebufferTexture2DMultisampleEXT)       /* 406 */ \
  OP(TexStorage2DEXT)                          /* 407 */ \
  OP(GenQueriesEXTImmediate)                   /* 408 */ \
  OP(DeleteQueriesEXTImmediate)                /* 409 */ \
  OP(BeginQueryEXT)                            /* 410 */ \
  OP(EndQueryEXT)                              /* 411 */ \
  OP(InsertEventMarkerEXT)                     /* 412 */ \
  OP(PushGroupMarkerEXT)                       /* 413 */ \
  OP(PopGroupMarkerEXT)                        /* 414 */ \
  OP(GenVertexArraysOESImmediate)              /* 415 */ \
  OP(DeleteVertexArraysOESImmediate)           /* 416 */ \
  OP(IsVertexArrayOES)                         /* 417 */ \
  OP(BindVertexArrayOES)                       /* 418 */ \
  OP(SwapBuffers)                              /* 419 */ \
  OP(GetMaxValueInBufferCHROMIUM)              /* 420 */ \
  OP(EnableFeatureCHROMIUM)                    /* 421 */ \
  OP(ResizeCHROMIUM)                           /* 422 */ \
  OP(GetRequestableExtensionsCHROMIUM)         /* 423 */ \
  OP(RequestExtensionCHROMIUM)                 /* 424 */ \
  OP(GetProgramInfoCHROMIUM)                   /* 425 */ \
  OP(GetTranslatedShaderSourceANGLE)           /* 426 */ \
  OP(PostSubBufferCHROMIUM)                    /* 427 */ \
  OP(TexImageIOSurface2DCHROMIUM)              /* 428 */ \
  OP(CopyTextureCHROMIUM)                      /* 429 */ \
  OP(DrawArraysInstancedANGLE)                 /* 430 */ \
  OP(DrawElementsInstancedANGLE)               /* 431 */ \
  OP(VertexAttribDivisorANGLE)                 /* 432 */ \
  OP(GenMailboxCHROMIUM)                       /* 433 */ \
  OP(ProduceTextureCHROMIUMImmediate)          /* 434 */ \
  OP(ProduceTextureDirectCHROMIUMImmediate)    /* 435 */ \
  OP(ConsumeTextureCHROMIUMImmediate)          /* 436 */ \
  OP(CreateAndConsumeTextureCHROMIUMImmediate) /* 437 */ \
  OP(BindUniformLocationCHROMIUMBucket)        /* 438 */ \
  OP(GenValuebuffersCHROMIUMImmediate)         /* 439 */ \
  OP(DeleteValuebuffersCHROMIUMImmediate)      /* 440 */ \
  OP(IsValuebufferCHROMIUM)                    /* 441 */ \
  OP(BindValuebufferCHROMIUM)                  /* 442 */ \
  OP(SubscribeValueCHROMIUM)                   /* 443 */ \
  OP(PopulateSubscribedValuesCHROMIUM)         /* 444 */ \
  OP(UniformValuebufferCHROMIUM)               /* 445 */ \
  OP(BindTexImage2DCHROMIUM)                   /* 446 */ \
  OP(ReleaseTexImage2DCHROMIUM)                /* 447 */ \
  OP(TraceBeginCHROMIUM)                       /* 448 */ \
  OP(TraceEndCHROMIUM)                         /* 449 */ \
  OP(AsyncTexSubImage2DCHROMIUM)               /* 450 */ \
  OP(AsyncTexImage2DCHROMIUM)                  /* 451 */ \
  OP(WaitAsyncTexImage2DCHROMIUM)              /* 452 */ \
  OP(WaitAllAsyncTexImage2DCHROMIUM)           /* 453 */ \
  OP(DiscardFramebufferEXTImmediate)           /* 454 */ \
  OP(LoseContextCHROMIUM)                      /* 455 */ \
  OP(InsertSyncPointCHROMIUM)                  /* 456 */ \
  OP(WaitSyncPointCHROMIUM)                    /* 457 */ \
  OP(DrawBuffersEXTImmediate)                  /* 458 */ \
  OP(DiscardBackbufferCHROMIUM)                /* 459 */ \
  OP(ScheduleOverlayPlaneCHROMIUM)             /* 460 */ \
  OP(MatrixLoadfCHROMIUMImmediate)             /* 461 */ \
  OP(MatrixLoadIdentityCHROMIUM)               /* 462 */ \
  OP(BlendBarrierKHR)                          /* 463 */

enum CommandId {
  kStartPoint = cmd::kLastCommonId,  // All GLES2 commands start after this.
#define GLES2_CMD_OP(name) k##name,
  GLES2_COMMAND_LIST(GLES2_CMD_OP)
#undef GLES2_CMD_OP
      kNumCommands
};

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_IDS_AUTOGEN_H_
