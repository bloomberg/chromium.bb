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
  OP(CopyTexImage2D)                           /* 281 */ \
  OP(CopyTexSubImage2D)                        /* 282 */ \
  OP(CreateProgram)                            /* 283 */ \
  OP(CreateShader)                             /* 284 */ \
  OP(CullFace)                                 /* 285 */ \
  OP(DeleteBuffersImmediate)                   /* 286 */ \
  OP(DeleteFramebuffersImmediate)              /* 287 */ \
  OP(DeleteProgram)                            /* 288 */ \
  OP(DeleteRenderbuffersImmediate)             /* 289 */ \
  OP(DeleteShader)                             /* 290 */ \
  OP(DeleteTexturesImmediate)                  /* 291 */ \
  OP(DepthFunc)                                /* 292 */ \
  OP(DepthMask)                                /* 293 */ \
  OP(DepthRangef)                              /* 294 */ \
  OP(DetachShader)                             /* 295 */ \
  OP(Disable)                                  /* 296 */ \
  OP(DisableVertexAttribArray)                 /* 297 */ \
  OP(DrawArrays)                               /* 298 */ \
  OP(DrawElements)                             /* 299 */ \
  OP(Enable)                                   /* 300 */ \
  OP(EnableVertexAttribArray)                  /* 301 */ \
  OP(Finish)                                   /* 302 */ \
  OP(Flush)                                    /* 303 */ \
  OP(FramebufferRenderbuffer)                  /* 304 */ \
  OP(FramebufferTexture2D)                     /* 305 */ \
  OP(FramebufferTextureLayer)                  /* 306 */ \
  OP(FrontFace)                                /* 307 */ \
  OP(GenBuffersImmediate)                      /* 308 */ \
  OP(GenerateMipmap)                           /* 309 */ \
  OP(GenFramebuffersImmediate)                 /* 310 */ \
  OP(GenRenderbuffersImmediate)                /* 311 */ \
  OP(GenTexturesImmediate)                     /* 312 */ \
  OP(GetActiveAttrib)                          /* 313 */ \
  OP(GetActiveUniform)                         /* 314 */ \
  OP(GetAttachedShaders)                       /* 315 */ \
  OP(GetAttribLocation)                        /* 316 */ \
  OP(GetBooleanv)                              /* 317 */ \
  OP(GetBufferParameteriv)                     /* 318 */ \
  OP(GetError)                                 /* 319 */ \
  OP(GetFloatv)                                /* 320 */ \
  OP(GetFramebufferAttachmentParameteriv)      /* 321 */ \
  OP(GetIntegerv)                              /* 322 */ \
  OP(GetProgramiv)                             /* 323 */ \
  OP(GetProgramInfoLog)                        /* 324 */ \
  OP(GetRenderbufferParameteriv)               /* 325 */ \
  OP(GetShaderiv)                              /* 326 */ \
  OP(GetShaderInfoLog)                         /* 327 */ \
  OP(GetShaderPrecisionFormat)                 /* 328 */ \
  OP(GetShaderSource)                          /* 329 */ \
  OP(GetString)                                /* 330 */ \
  OP(GetTexParameterfv)                        /* 331 */ \
  OP(GetTexParameteriv)                        /* 332 */ \
  OP(GetUniformfv)                             /* 333 */ \
  OP(GetUniformiv)                             /* 334 */ \
  OP(GetUniformLocation)                       /* 335 */ \
  OP(GetVertexAttribfv)                        /* 336 */ \
  OP(GetVertexAttribiv)                        /* 337 */ \
  OP(GetVertexAttribPointerv)                  /* 338 */ \
  OP(Hint)                                     /* 339 */ \
  OP(IsBuffer)                                 /* 340 */ \
  OP(IsEnabled)                                /* 341 */ \
  OP(IsFramebuffer)                            /* 342 */ \
  OP(IsProgram)                                /* 343 */ \
  OP(IsRenderbuffer)                           /* 344 */ \
  OP(IsShader)                                 /* 345 */ \
  OP(IsTexture)                                /* 346 */ \
  OP(LineWidth)                                /* 347 */ \
  OP(LinkProgram)                              /* 348 */ \
  OP(PixelStorei)                              /* 349 */ \
  OP(PolygonOffset)                            /* 350 */ \
  OP(ReadPixels)                               /* 351 */ \
  OP(ReleaseShaderCompiler)                    /* 352 */ \
  OP(RenderbufferStorage)                      /* 353 */ \
  OP(SampleCoverage)                           /* 354 */ \
  OP(Scissor)                                  /* 355 */ \
  OP(ShaderBinary)                             /* 356 */ \
  OP(ShaderSourceBucket)                       /* 357 */ \
  OP(StencilFunc)                              /* 358 */ \
  OP(StencilFuncSeparate)                      /* 359 */ \
  OP(StencilMask)                              /* 360 */ \
  OP(StencilMaskSeparate)                      /* 361 */ \
  OP(StencilOp)                                /* 362 */ \
  OP(StencilOpSeparate)                        /* 363 */ \
  OP(TexImage2D)                               /* 364 */ \
  OP(TexParameterf)                            /* 365 */ \
  OP(TexParameterfvImmediate)                  /* 366 */ \
  OP(TexParameteri)                            /* 367 */ \
  OP(TexParameterivImmediate)                  /* 368 */ \
  OP(TexSubImage2D)                            /* 369 */ \
  OP(Uniform1f)                                /* 370 */ \
  OP(Uniform1fvImmediate)                      /* 371 */ \
  OP(Uniform1i)                                /* 372 */ \
  OP(Uniform1ivImmediate)                      /* 373 */ \
  OP(Uniform2f)                                /* 374 */ \
  OP(Uniform2fvImmediate)                      /* 375 */ \
  OP(Uniform2i)                                /* 376 */ \
  OP(Uniform2ivImmediate)                      /* 377 */ \
  OP(Uniform3f)                                /* 378 */ \
  OP(Uniform3fvImmediate)                      /* 379 */ \
  OP(Uniform3i)                                /* 380 */ \
  OP(Uniform3ivImmediate)                      /* 381 */ \
  OP(Uniform4f)                                /* 382 */ \
  OP(Uniform4fvImmediate)                      /* 383 */ \
  OP(Uniform4i)                                /* 384 */ \
  OP(Uniform4ivImmediate)                      /* 385 */ \
  OP(UniformMatrix2fvImmediate)                /* 386 */ \
  OP(UniformMatrix3fvImmediate)                /* 387 */ \
  OP(UniformMatrix4fvImmediate)                /* 388 */ \
  OP(UseProgram)                               /* 389 */ \
  OP(ValidateProgram)                          /* 390 */ \
  OP(VertexAttrib1f)                           /* 391 */ \
  OP(VertexAttrib1fvImmediate)                 /* 392 */ \
  OP(VertexAttrib2f)                           /* 393 */ \
  OP(VertexAttrib2fvImmediate)                 /* 394 */ \
  OP(VertexAttrib3f)                           /* 395 */ \
  OP(VertexAttrib3fvImmediate)                 /* 396 */ \
  OP(VertexAttrib4f)                           /* 397 */ \
  OP(VertexAttrib4fvImmediate)                 /* 398 */ \
  OP(VertexAttribPointer)                      /* 399 */ \
  OP(Viewport)                                 /* 400 */ \
  OP(BlitFramebufferCHROMIUM)                  /* 401 */ \
  OP(RenderbufferStorageMultisampleCHROMIUM)   /* 402 */ \
  OP(RenderbufferStorageMultisampleEXT)        /* 403 */ \
  OP(FramebufferTexture2DMultisampleEXT)       /* 404 */ \
  OP(TexStorage2DEXT)                          /* 405 */ \
  OP(GenQueriesEXTImmediate)                   /* 406 */ \
  OP(DeleteQueriesEXTImmediate)                /* 407 */ \
  OP(BeginQueryEXT)                            /* 408 */ \
  OP(EndQueryEXT)                              /* 409 */ \
  OP(InsertEventMarkerEXT)                     /* 410 */ \
  OP(PushGroupMarkerEXT)                       /* 411 */ \
  OP(PopGroupMarkerEXT)                        /* 412 */ \
  OP(GenVertexArraysOESImmediate)              /* 413 */ \
  OP(DeleteVertexArraysOESImmediate)           /* 414 */ \
  OP(IsVertexArrayOES)                         /* 415 */ \
  OP(BindVertexArrayOES)                       /* 416 */ \
  OP(SwapBuffers)                              /* 417 */ \
  OP(GetMaxValueInBufferCHROMIUM)              /* 418 */ \
  OP(EnableFeatureCHROMIUM)                    /* 419 */ \
  OP(ResizeCHROMIUM)                           /* 420 */ \
  OP(GetRequestableExtensionsCHROMIUM)         /* 421 */ \
  OP(RequestExtensionCHROMIUM)                 /* 422 */ \
  OP(GetProgramInfoCHROMIUM)                   /* 423 */ \
  OP(GetTranslatedShaderSourceANGLE)           /* 424 */ \
  OP(PostSubBufferCHROMIUM)                    /* 425 */ \
  OP(TexImageIOSurface2DCHROMIUM)              /* 426 */ \
  OP(CopyTextureCHROMIUM)                      /* 427 */ \
  OP(DrawArraysInstancedANGLE)                 /* 428 */ \
  OP(DrawElementsInstancedANGLE)               /* 429 */ \
  OP(VertexAttribDivisorANGLE)                 /* 430 */ \
  OP(GenMailboxCHROMIUM)                       /* 431 */ \
  OP(ProduceTextureCHROMIUMImmediate)          /* 432 */ \
  OP(ProduceTextureDirectCHROMIUMImmediate)    /* 433 */ \
  OP(ConsumeTextureCHROMIUMImmediate)          /* 434 */ \
  OP(CreateAndConsumeTextureCHROMIUMImmediate) /* 435 */ \
  OP(BindUniformLocationCHROMIUMBucket)        /* 436 */ \
  OP(GenValuebuffersCHROMIUMImmediate)         /* 437 */ \
  OP(DeleteValuebuffersCHROMIUMImmediate)      /* 438 */ \
  OP(IsValuebufferCHROMIUM)                    /* 439 */ \
  OP(BindValuebufferCHROMIUM)                  /* 440 */ \
  OP(SubscribeValueCHROMIUM)                   /* 441 */ \
  OP(PopulateSubscribedValuesCHROMIUM)         /* 442 */ \
  OP(UniformValuebufferCHROMIUM)               /* 443 */ \
  OP(BindTexImage2DCHROMIUM)                   /* 444 */ \
  OP(ReleaseTexImage2DCHROMIUM)                /* 445 */ \
  OP(TraceBeginCHROMIUM)                       /* 446 */ \
  OP(TraceEndCHROMIUM)                         /* 447 */ \
  OP(AsyncTexSubImage2DCHROMIUM)               /* 448 */ \
  OP(AsyncTexImage2DCHROMIUM)                  /* 449 */ \
  OP(WaitAsyncTexImage2DCHROMIUM)              /* 450 */ \
  OP(WaitAllAsyncTexImage2DCHROMIUM)           /* 451 */ \
  OP(DiscardFramebufferEXTImmediate)           /* 452 */ \
  OP(LoseContextCHROMIUM)                      /* 453 */ \
  OP(InsertSyncPointCHROMIUM)                  /* 454 */ \
  OP(WaitSyncPointCHROMIUM)                    /* 455 */ \
  OP(DrawBuffersEXTImmediate)                  /* 456 */ \
  OP(DiscardBackbufferCHROMIUM)                /* 457 */ \
  OP(ScheduleOverlayPlaneCHROMIUM)             /* 458 */ \
  OP(MatrixLoadfCHROMIUMImmediate)             /* 459 */ \
  OP(MatrixLoadIdentityCHROMIUM)               /* 460 */ \
  OP(BlendBarrierKHR)                          /* 461 */

enum CommandId {
  kStartPoint = cmd::kLastCommonId,  // All GLES2 commands start after this.
#define GLES2_CMD_OP(name) k##name,
  GLES2_COMMAND_LIST(GLES2_CMD_OP)
#undef GLES2_CMD_OP
      kNumCommands
};

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_IDS_AUTOGEN_H_
