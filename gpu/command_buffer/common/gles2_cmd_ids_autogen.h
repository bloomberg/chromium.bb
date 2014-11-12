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
  OP(FrontFace)                                /* 306 */ \
  OP(GenBuffersImmediate)                      /* 307 */ \
  OP(GenerateMipmap)                           /* 308 */ \
  OP(GenFramebuffersImmediate)                 /* 309 */ \
  OP(GenRenderbuffersImmediate)                /* 310 */ \
  OP(GenTexturesImmediate)                     /* 311 */ \
  OP(GetActiveAttrib)                          /* 312 */ \
  OP(GetActiveUniform)                         /* 313 */ \
  OP(GetAttachedShaders)                       /* 314 */ \
  OP(GetAttribLocation)                        /* 315 */ \
  OP(GetBooleanv)                              /* 316 */ \
  OP(GetBufferParameteriv)                     /* 317 */ \
  OP(GetError)                                 /* 318 */ \
  OP(GetFloatv)                                /* 319 */ \
  OP(GetFramebufferAttachmentParameteriv)      /* 320 */ \
  OP(GetIntegerv)                              /* 321 */ \
  OP(GetProgramiv)                             /* 322 */ \
  OP(GetProgramInfoLog)                        /* 323 */ \
  OP(GetRenderbufferParameteriv)               /* 324 */ \
  OP(GetShaderiv)                              /* 325 */ \
  OP(GetShaderInfoLog)                         /* 326 */ \
  OP(GetShaderPrecisionFormat)                 /* 327 */ \
  OP(GetShaderSource)                          /* 328 */ \
  OP(GetString)                                /* 329 */ \
  OP(GetTexParameterfv)                        /* 330 */ \
  OP(GetTexParameteriv)                        /* 331 */ \
  OP(GetUniformfv)                             /* 332 */ \
  OP(GetUniformiv)                             /* 333 */ \
  OP(GetUniformLocation)                       /* 334 */ \
  OP(GetVertexAttribfv)                        /* 335 */ \
  OP(GetVertexAttribiv)                        /* 336 */ \
  OP(GetVertexAttribPointerv)                  /* 337 */ \
  OP(Hint)                                     /* 338 */ \
  OP(IsBuffer)                                 /* 339 */ \
  OP(IsEnabled)                                /* 340 */ \
  OP(IsFramebuffer)                            /* 341 */ \
  OP(IsProgram)                                /* 342 */ \
  OP(IsRenderbuffer)                           /* 343 */ \
  OP(IsShader)                                 /* 344 */ \
  OP(IsTexture)                                /* 345 */ \
  OP(LineWidth)                                /* 346 */ \
  OP(LinkProgram)                              /* 347 */ \
  OP(PixelStorei)                              /* 348 */ \
  OP(PolygonOffset)                            /* 349 */ \
  OP(ReadPixels)                               /* 350 */ \
  OP(ReleaseShaderCompiler)                    /* 351 */ \
  OP(RenderbufferStorage)                      /* 352 */ \
  OP(SampleCoverage)                           /* 353 */ \
  OP(Scissor)                                  /* 354 */ \
  OP(ShaderBinary)                             /* 355 */ \
  OP(ShaderSourceBucket)                       /* 356 */ \
  OP(StencilFunc)                              /* 357 */ \
  OP(StencilFuncSeparate)                      /* 358 */ \
  OP(StencilMask)                              /* 359 */ \
  OP(StencilMaskSeparate)                      /* 360 */ \
  OP(StencilOp)                                /* 361 */ \
  OP(StencilOpSeparate)                        /* 362 */ \
  OP(TexImage2D)                               /* 363 */ \
  OP(TexParameterf)                            /* 364 */ \
  OP(TexParameterfvImmediate)                  /* 365 */ \
  OP(TexParameteri)                            /* 366 */ \
  OP(TexParameterivImmediate)                  /* 367 */ \
  OP(TexSubImage2D)                            /* 368 */ \
  OP(Uniform1f)                                /* 369 */ \
  OP(Uniform1fvImmediate)                      /* 370 */ \
  OP(Uniform1i)                                /* 371 */ \
  OP(Uniform1ivImmediate)                      /* 372 */ \
  OP(Uniform2f)                                /* 373 */ \
  OP(Uniform2fvImmediate)                      /* 374 */ \
  OP(Uniform2i)                                /* 375 */ \
  OP(Uniform2ivImmediate)                      /* 376 */ \
  OP(Uniform3f)                                /* 377 */ \
  OP(Uniform3fvImmediate)                      /* 378 */ \
  OP(Uniform3i)                                /* 379 */ \
  OP(Uniform3ivImmediate)                      /* 380 */ \
  OP(Uniform4f)                                /* 381 */ \
  OP(Uniform4fvImmediate)                      /* 382 */ \
  OP(Uniform4i)                                /* 383 */ \
  OP(Uniform4ivImmediate)                      /* 384 */ \
  OP(UniformMatrix2fvImmediate)                /* 385 */ \
  OP(UniformMatrix3fvImmediate)                /* 386 */ \
  OP(UniformMatrix4fvImmediate)                /* 387 */ \
  OP(UseProgram)                               /* 388 */ \
  OP(ValidateProgram)                          /* 389 */ \
  OP(VertexAttrib1f)                           /* 390 */ \
  OP(VertexAttrib1fvImmediate)                 /* 391 */ \
  OP(VertexAttrib2f)                           /* 392 */ \
  OP(VertexAttrib2fvImmediate)                 /* 393 */ \
  OP(VertexAttrib3f)                           /* 394 */ \
  OP(VertexAttrib3fvImmediate)                 /* 395 */ \
  OP(VertexAttrib4f)                           /* 396 */ \
  OP(VertexAttrib4fvImmediate)                 /* 397 */ \
  OP(VertexAttribPointer)                      /* 398 */ \
  OP(Viewport)                                 /* 399 */ \
  OP(BlitFramebufferCHROMIUM)                  /* 400 */ \
  OP(RenderbufferStorageMultisampleCHROMIUM)   /* 401 */ \
  OP(RenderbufferStorageMultisampleEXT)        /* 402 */ \
  OP(FramebufferTexture2DMultisampleEXT)       /* 403 */ \
  OP(TexStorage2DEXT)                          /* 404 */ \
  OP(GenQueriesEXTImmediate)                   /* 405 */ \
  OP(DeleteQueriesEXTImmediate)                /* 406 */ \
  OP(BeginQueryEXT)                            /* 407 */ \
  OP(EndQueryEXT)                              /* 408 */ \
  OP(InsertEventMarkerEXT)                     /* 409 */ \
  OP(PushGroupMarkerEXT)                       /* 410 */ \
  OP(PopGroupMarkerEXT)                        /* 411 */ \
  OP(GenVertexArraysOESImmediate)              /* 412 */ \
  OP(DeleteVertexArraysOESImmediate)           /* 413 */ \
  OP(IsVertexArrayOES)                         /* 414 */ \
  OP(BindVertexArrayOES)                       /* 415 */ \
  OP(SwapBuffers)                              /* 416 */ \
  OP(GetMaxValueInBufferCHROMIUM)              /* 417 */ \
  OP(EnableFeatureCHROMIUM)                    /* 418 */ \
  OP(ResizeCHROMIUM)                           /* 419 */ \
  OP(GetRequestableExtensionsCHROMIUM)         /* 420 */ \
  OP(RequestExtensionCHROMIUM)                 /* 421 */ \
  OP(GetProgramInfoCHROMIUM)                   /* 422 */ \
  OP(GetTranslatedShaderSourceANGLE)           /* 423 */ \
  OP(PostSubBufferCHROMIUM)                    /* 424 */ \
  OP(TexImageIOSurface2DCHROMIUM)              /* 425 */ \
  OP(CopyTextureCHROMIUM)                      /* 426 */ \
  OP(DrawArraysInstancedANGLE)                 /* 427 */ \
  OP(DrawElementsInstancedANGLE)               /* 428 */ \
  OP(VertexAttribDivisorANGLE)                 /* 429 */ \
  OP(GenMailboxCHROMIUM)                       /* 430 */ \
  OP(ProduceTextureCHROMIUMImmediate)          /* 431 */ \
  OP(ProduceTextureDirectCHROMIUMImmediate)    /* 432 */ \
  OP(ConsumeTextureCHROMIUMImmediate)          /* 433 */ \
  OP(CreateAndConsumeTextureCHROMIUMImmediate) /* 434 */ \
  OP(BindUniformLocationCHROMIUMBucket)        /* 435 */ \
  OP(GenValuebuffersCHROMIUMImmediate)         /* 436 */ \
  OP(DeleteValuebuffersCHROMIUMImmediate)      /* 437 */ \
  OP(IsValuebufferCHROMIUM)                    /* 438 */ \
  OP(BindValuebufferCHROMIUM)                  /* 439 */ \
  OP(SubscribeValueCHROMIUM)                   /* 440 */ \
  OP(PopulateSubscribedValuesCHROMIUM)         /* 441 */ \
  OP(UniformValuebufferCHROMIUM)               /* 442 */ \
  OP(BindTexImage2DCHROMIUM)                   /* 443 */ \
  OP(ReleaseTexImage2DCHROMIUM)                /* 444 */ \
  OP(TraceBeginCHROMIUM)                       /* 445 */ \
  OP(TraceEndCHROMIUM)                         /* 446 */ \
  OP(AsyncTexSubImage2DCHROMIUM)               /* 447 */ \
  OP(AsyncTexImage2DCHROMIUM)                  /* 448 */ \
  OP(WaitAsyncTexImage2DCHROMIUM)              /* 449 */ \
  OP(WaitAllAsyncTexImage2DCHROMIUM)           /* 450 */ \
  OP(DiscardFramebufferEXTImmediate)           /* 451 */ \
  OP(LoseContextCHROMIUM)                      /* 452 */ \
  OP(InsertSyncPointCHROMIUM)                  /* 453 */ \
  OP(WaitSyncPointCHROMIUM)                    /* 454 */ \
  OP(DrawBuffersEXTImmediate)                  /* 455 */ \
  OP(DiscardBackbufferCHROMIUM)                /* 456 */ \
  OP(ScheduleOverlayPlaneCHROMIUM)             /* 457 */ \
  OP(MatrixLoadfCHROMIUMImmediate)             /* 458 */ \
  OP(MatrixLoadIdentityCHROMIUM)               /* 459 */ \
  OP(BlendBarrierKHR)                          /* 460 */

enum CommandId {
  kStartPoint = cmd::kLastCommonId,  // All GLES2 commands start after this.
#define GLES2_CMD_OP(name) k##name,
  GLES2_COMMAND_LIST(GLES2_CMD_OP)
#undef GLES2_CMD_OP
      kNumCommands
};

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_IDS_AUTOGEN_H_
