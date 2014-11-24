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
  OP(GetProgramiv)                             /* 324 */ \
  OP(GetProgramInfoLog)                        /* 325 */ \
  OP(GetRenderbufferParameteriv)               /* 326 */ \
  OP(GetShaderiv)                              /* 327 */ \
  OP(GetShaderInfoLog)                         /* 328 */ \
  OP(GetShaderPrecisionFormat)                 /* 329 */ \
  OP(GetShaderSource)                          /* 330 */ \
  OP(GetString)                                /* 331 */ \
  OP(GetTexParameterfv)                        /* 332 */ \
  OP(GetTexParameteriv)                        /* 333 */ \
  OP(GetUniformfv)                             /* 334 */ \
  OP(GetUniformiv)                             /* 335 */ \
  OP(GetUniformLocation)                       /* 336 */ \
  OP(GetVertexAttribfv)                        /* 337 */ \
  OP(GetVertexAttribiv)                        /* 338 */ \
  OP(GetVertexAttribPointerv)                  /* 339 */ \
  OP(Hint)                                     /* 340 */ \
  OP(IsBuffer)                                 /* 341 */ \
  OP(IsEnabled)                                /* 342 */ \
  OP(IsFramebuffer)                            /* 343 */ \
  OP(IsProgram)                                /* 344 */ \
  OP(IsRenderbuffer)                           /* 345 */ \
  OP(IsShader)                                 /* 346 */ \
  OP(IsTexture)                                /* 347 */ \
  OP(LineWidth)                                /* 348 */ \
  OP(LinkProgram)                              /* 349 */ \
  OP(PixelStorei)                              /* 350 */ \
  OP(PolygonOffset)                            /* 351 */ \
  OP(ReadPixels)                               /* 352 */ \
  OP(ReleaseShaderCompiler)                    /* 353 */ \
  OP(RenderbufferStorage)                      /* 354 */ \
  OP(SampleCoverage)                           /* 355 */ \
  OP(Scissor)                                  /* 356 */ \
  OP(ShaderBinary)                             /* 357 */ \
  OP(ShaderSourceBucket)                       /* 358 */ \
  OP(StencilFunc)                              /* 359 */ \
  OP(StencilFuncSeparate)                      /* 360 */ \
  OP(StencilMask)                              /* 361 */ \
  OP(StencilMaskSeparate)                      /* 362 */ \
  OP(StencilOp)                                /* 363 */ \
  OP(StencilOpSeparate)                        /* 364 */ \
  OP(TexImage2D)                               /* 365 */ \
  OP(TexParameterf)                            /* 366 */ \
  OP(TexParameterfvImmediate)                  /* 367 */ \
  OP(TexParameteri)                            /* 368 */ \
  OP(TexParameterivImmediate)                  /* 369 */ \
  OP(TexSubImage2D)                            /* 370 */ \
  OP(Uniform1f)                                /* 371 */ \
  OP(Uniform1fvImmediate)                      /* 372 */ \
  OP(Uniform1i)                                /* 373 */ \
  OP(Uniform1ivImmediate)                      /* 374 */ \
  OP(Uniform2f)                                /* 375 */ \
  OP(Uniform2fvImmediate)                      /* 376 */ \
  OP(Uniform2i)                                /* 377 */ \
  OP(Uniform2ivImmediate)                      /* 378 */ \
  OP(Uniform3f)                                /* 379 */ \
  OP(Uniform3fvImmediate)                      /* 380 */ \
  OP(Uniform3i)                                /* 381 */ \
  OP(Uniform3ivImmediate)                      /* 382 */ \
  OP(Uniform4f)                                /* 383 */ \
  OP(Uniform4fvImmediate)                      /* 384 */ \
  OP(Uniform4i)                                /* 385 */ \
  OP(Uniform4ivImmediate)                      /* 386 */ \
  OP(UniformMatrix2fvImmediate)                /* 387 */ \
  OP(UniformMatrix3fvImmediate)                /* 388 */ \
  OP(UniformMatrix4fvImmediate)                /* 389 */ \
  OP(UseProgram)                               /* 390 */ \
  OP(ValidateProgram)                          /* 391 */ \
  OP(VertexAttrib1f)                           /* 392 */ \
  OP(VertexAttrib1fvImmediate)                 /* 393 */ \
  OP(VertexAttrib2f)                           /* 394 */ \
  OP(VertexAttrib2fvImmediate)                 /* 395 */ \
  OP(VertexAttrib3f)                           /* 396 */ \
  OP(VertexAttrib3fvImmediate)                 /* 397 */ \
  OP(VertexAttrib4f)                           /* 398 */ \
  OP(VertexAttrib4fvImmediate)                 /* 399 */ \
  OP(VertexAttribPointer)                      /* 400 */ \
  OP(Viewport)                                 /* 401 */ \
  OP(BlitFramebufferCHROMIUM)                  /* 402 */ \
  OP(RenderbufferStorageMultisampleCHROMIUM)   /* 403 */ \
  OP(RenderbufferStorageMultisampleEXT)        /* 404 */ \
  OP(FramebufferTexture2DMultisampleEXT)       /* 405 */ \
  OP(TexStorage2DEXT)                          /* 406 */ \
  OP(GenQueriesEXTImmediate)                   /* 407 */ \
  OP(DeleteQueriesEXTImmediate)                /* 408 */ \
  OP(BeginQueryEXT)                            /* 409 */ \
  OP(EndQueryEXT)                              /* 410 */ \
  OP(InsertEventMarkerEXT)                     /* 411 */ \
  OP(PushGroupMarkerEXT)                       /* 412 */ \
  OP(PopGroupMarkerEXT)                        /* 413 */ \
  OP(GenVertexArraysOESImmediate)              /* 414 */ \
  OP(DeleteVertexArraysOESImmediate)           /* 415 */ \
  OP(IsVertexArrayOES)                         /* 416 */ \
  OP(BindVertexArrayOES)                       /* 417 */ \
  OP(SwapBuffers)                              /* 418 */ \
  OP(GetMaxValueInBufferCHROMIUM)              /* 419 */ \
  OP(EnableFeatureCHROMIUM)                    /* 420 */ \
  OP(ResizeCHROMIUM)                           /* 421 */ \
  OP(GetRequestableExtensionsCHROMIUM)         /* 422 */ \
  OP(RequestExtensionCHROMIUM)                 /* 423 */ \
  OP(GetProgramInfoCHROMIUM)                   /* 424 */ \
  OP(GetTranslatedShaderSourceANGLE)           /* 425 */ \
  OP(PostSubBufferCHROMIUM)                    /* 426 */ \
  OP(TexImageIOSurface2DCHROMIUM)              /* 427 */ \
  OP(CopyTextureCHROMIUM)                      /* 428 */ \
  OP(DrawArraysInstancedANGLE)                 /* 429 */ \
  OP(DrawElementsInstancedANGLE)               /* 430 */ \
  OP(VertexAttribDivisorANGLE)                 /* 431 */ \
  OP(GenMailboxCHROMIUM)                       /* 432 */ \
  OP(ProduceTextureCHROMIUMImmediate)          /* 433 */ \
  OP(ProduceTextureDirectCHROMIUMImmediate)    /* 434 */ \
  OP(ConsumeTextureCHROMIUMImmediate)          /* 435 */ \
  OP(CreateAndConsumeTextureCHROMIUMImmediate) /* 436 */ \
  OP(BindUniformLocationCHROMIUMBucket)        /* 437 */ \
  OP(GenValuebuffersCHROMIUMImmediate)         /* 438 */ \
  OP(DeleteValuebuffersCHROMIUMImmediate)      /* 439 */ \
  OP(IsValuebufferCHROMIUM)                    /* 440 */ \
  OP(BindValuebufferCHROMIUM)                  /* 441 */ \
  OP(SubscribeValueCHROMIUM)                   /* 442 */ \
  OP(PopulateSubscribedValuesCHROMIUM)         /* 443 */ \
  OP(UniformValuebufferCHROMIUM)               /* 444 */ \
  OP(BindTexImage2DCHROMIUM)                   /* 445 */ \
  OP(ReleaseTexImage2DCHROMIUM)                /* 446 */ \
  OP(TraceBeginCHROMIUM)                       /* 447 */ \
  OP(TraceEndCHROMIUM)                         /* 448 */ \
  OP(AsyncTexSubImage2DCHROMIUM)               /* 449 */ \
  OP(AsyncTexImage2DCHROMIUM)                  /* 450 */ \
  OP(WaitAsyncTexImage2DCHROMIUM)              /* 451 */ \
  OP(WaitAllAsyncTexImage2DCHROMIUM)           /* 452 */ \
  OP(DiscardFramebufferEXTImmediate)           /* 453 */ \
  OP(LoseContextCHROMIUM)                      /* 454 */ \
  OP(InsertSyncPointCHROMIUM)                  /* 455 */ \
  OP(WaitSyncPointCHROMIUM)                    /* 456 */ \
  OP(DrawBuffersEXTImmediate)                  /* 457 */ \
  OP(DiscardBackbufferCHROMIUM)                /* 458 */ \
  OP(ScheduleOverlayPlaneCHROMIUM)             /* 459 */ \
  OP(MatrixLoadfCHROMIUMImmediate)             /* 460 */ \
  OP(MatrixLoadIdentityCHROMIUM)               /* 461 */ \
  OP(BlendBarrierKHR)                          /* 462 */

enum CommandId {
  kStartPoint = cmd::kLastCommonId,  // All GLES2 commands start after this.
#define GLES2_CMD_OP(name) k##name,
  GLES2_COMMAND_LIST(GLES2_CMD_OP)
#undef GLES2_CMD_OP
      kNumCommands
};

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_IDS_AUTOGEN_H_
