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

#define GLES2_COMMAND_LIST(OP)                         \
  OP(ActiveTexture)                          /* 256 */ \
  OP(AttachShader)                           /* 257 */ \
  OP(BindAttribLocation)                     /* 258 */ \
  OP(BindAttribLocationBucket)               /* 259 */ \
  OP(BindBuffer)                             /* 260 */ \
  OP(BindFramebuffer)                        /* 261 */ \
  OP(BindRenderbuffer)                       /* 262 */ \
  OP(BindTexture)                            /* 263 */ \
  OP(BlendColor)                             /* 264 */ \
  OP(BlendEquation)                          /* 265 */ \
  OP(BlendEquationSeparate)                  /* 266 */ \
  OP(BlendFunc)                              /* 267 */ \
  OP(BlendFuncSeparate)                      /* 268 */ \
  OP(BufferData)                             /* 269 */ \
  OP(BufferSubData)                          /* 270 */ \
  OP(CheckFramebufferStatus)                 /* 271 */ \
  OP(Clear)                                  /* 272 */ \
  OP(ClearColor)                             /* 273 */ \
  OP(ClearDepthf)                            /* 274 */ \
  OP(ClearStencil)                           /* 275 */ \
  OP(ColorMask)                              /* 276 */ \
  OP(CompileShader)                          /* 277 */ \
  OP(CompressedTexImage2D)                   /* 278 */ \
  OP(CompressedTexImage2DBucket)             /* 279 */ \
  OP(CompressedTexSubImage2D)                /* 280 */ \
  OP(CompressedTexSubImage2DBucket)          /* 281 */ \
  OP(CopyTexImage2D)                         /* 282 */ \
  OP(CopyTexSubImage2D)                      /* 283 */ \
  OP(CreateProgram)                          /* 284 */ \
  OP(CreateShader)                           /* 285 */ \
  OP(CullFace)                               /* 286 */ \
  OP(DeleteBuffersImmediate)                 /* 287 */ \
  OP(DeleteFramebuffersImmediate)            /* 288 */ \
  OP(DeleteProgram)                          /* 289 */ \
  OP(DeleteRenderbuffersImmediate)           /* 290 */ \
  OP(DeleteShader)                           /* 291 */ \
  OP(DeleteTexturesImmediate)                /* 292 */ \
  OP(DepthFunc)                              /* 293 */ \
  OP(DepthMask)                              /* 294 */ \
  OP(DepthRangef)                            /* 295 */ \
  OP(DetachShader)                           /* 296 */ \
  OP(Disable)                                /* 297 */ \
  OP(DisableVertexAttribArray)               /* 298 */ \
  OP(DrawArrays)                             /* 299 */ \
  OP(DrawElements)                           /* 300 */ \
  OP(Enable)                                 /* 301 */ \
  OP(EnableVertexAttribArray)                /* 302 */ \
  OP(Finish)                                 /* 303 */ \
  OP(Flush)                                  /* 304 */ \
  OP(FramebufferRenderbuffer)                /* 305 */ \
  OP(FramebufferTexture2D)                   /* 306 */ \
  OP(FrontFace)                              /* 307 */ \
  OP(GenBuffersImmediate)                    /* 308 */ \
  OP(GenerateMipmap)                         /* 309 */ \
  OP(GenFramebuffersImmediate)               /* 310 */ \
  OP(GenRenderbuffersImmediate)              /* 311 */ \
  OP(GenTexturesImmediate)                   /* 312 */ \
  OP(GetActiveAttrib)                        /* 313 */ \
  OP(GetActiveUniform)                       /* 314 */ \
  OP(GetAttachedShaders)                     /* 315 */ \
  OP(GetAttribLocation)                      /* 316 */ \
  OP(GetAttribLocationBucket)                /* 317 */ \
  OP(GetBooleanv)                            /* 318 */ \
  OP(GetBufferParameteriv)                   /* 319 */ \
  OP(GetError)                               /* 320 */ \
  OP(GetFloatv)                              /* 321 */ \
  OP(GetFramebufferAttachmentParameteriv)    /* 322 */ \
  OP(GetIntegerv)                            /* 323 */ \
  OP(GetProgramiv)                           /* 324 */ \
  OP(GetProgramInfoLog)                      /* 325 */ \
  OP(GetRenderbufferParameteriv)             /* 326 */ \
  OP(GetShaderiv)                            /* 327 */ \
  OP(GetShaderInfoLog)                       /* 328 */ \
  OP(GetShaderPrecisionFormat)               /* 329 */ \
  OP(GetShaderSource)                        /* 330 */ \
  OP(GetString)                              /* 331 */ \
  OP(GetTexParameterfv)                      /* 332 */ \
  OP(GetTexParameteriv)                      /* 333 */ \
  OP(GetUniformfv)                           /* 334 */ \
  OP(GetUniformiv)                           /* 335 */ \
  OP(GetUniformLocation)                     /* 336 */ \
  OP(GetUniformLocationBucket)               /* 337 */ \
  OP(GetVertexAttribfv)                      /* 338 */ \
  OP(GetVertexAttribiv)                      /* 339 */ \
  OP(GetVertexAttribPointerv)                /* 340 */ \
  OP(Hint)                                   /* 341 */ \
  OP(IsBuffer)                               /* 342 */ \
  OP(IsEnabled)                              /* 343 */ \
  OP(IsFramebuffer)                          /* 344 */ \
  OP(IsProgram)                              /* 345 */ \
  OP(IsRenderbuffer)                         /* 346 */ \
  OP(IsShader)                               /* 347 */ \
  OP(IsTexture)                              /* 348 */ \
  OP(LineWidth)                              /* 349 */ \
  OP(LinkProgram)                            /* 350 */ \
  OP(PixelStorei)                            /* 351 */ \
  OP(PolygonOffset)                          /* 352 */ \
  OP(ReadPixels)                             /* 353 */ \
  OP(ReleaseShaderCompiler)                  /* 354 */ \
  OP(RenderbufferStorage)                    /* 355 */ \
  OP(SampleCoverage)                         /* 356 */ \
  OP(Scissor)                                /* 357 */ \
  OP(ShaderBinary)                           /* 358 */ \
  OP(ShaderSource)                           /* 359 */ \
  OP(ShaderSourceBucket)                     /* 360 */ \
  OP(StencilFunc)                            /* 361 */ \
  OP(StencilFuncSeparate)                    /* 362 */ \
  OP(StencilMask)                            /* 363 */ \
  OP(StencilMaskSeparate)                    /* 364 */ \
  OP(StencilOp)                              /* 365 */ \
  OP(StencilOpSeparate)                      /* 366 */ \
  OP(TexImage2D)                             /* 367 */ \
  OP(TexParameterf)                          /* 368 */ \
  OP(TexParameterfvImmediate)                /* 369 */ \
  OP(TexParameteri)                          /* 370 */ \
  OP(TexParameterivImmediate)                /* 371 */ \
  OP(TexSubImage2D)                          /* 372 */ \
  OP(Uniform1f)                              /* 373 */ \
  OP(Uniform1fvImmediate)                    /* 374 */ \
  OP(Uniform1i)                              /* 375 */ \
  OP(Uniform1ivImmediate)                    /* 376 */ \
  OP(Uniform2f)                              /* 377 */ \
  OP(Uniform2fvImmediate)                    /* 378 */ \
  OP(Uniform2i)                              /* 379 */ \
  OP(Uniform2ivImmediate)                    /* 380 */ \
  OP(Uniform3f)                              /* 381 */ \
  OP(Uniform3fvImmediate)                    /* 382 */ \
  OP(Uniform3i)                              /* 383 */ \
  OP(Uniform3ivImmediate)                    /* 384 */ \
  OP(Uniform4f)                              /* 385 */ \
  OP(Uniform4fvImmediate)                    /* 386 */ \
  OP(Uniform4i)                              /* 387 */ \
  OP(Uniform4ivImmediate)                    /* 388 */ \
  OP(UniformMatrix2fvImmediate)              /* 389 */ \
  OP(UniformMatrix3fvImmediate)              /* 390 */ \
  OP(UniformMatrix4fvImmediate)              /* 391 */ \
  OP(UseProgram)                             /* 392 */ \
  OP(ValidateProgram)                        /* 393 */ \
  OP(VertexAttrib1f)                         /* 394 */ \
  OP(VertexAttrib1fvImmediate)               /* 395 */ \
  OP(VertexAttrib2f)                         /* 396 */ \
  OP(VertexAttrib2fvImmediate)               /* 397 */ \
  OP(VertexAttrib3f)                         /* 398 */ \
  OP(VertexAttrib3fvImmediate)               /* 399 */ \
  OP(VertexAttrib4f)                         /* 400 */ \
  OP(VertexAttrib4fvImmediate)               /* 401 */ \
  OP(VertexAttribPointer)                    /* 402 */ \
  OP(Viewport)                               /* 403 */ \
  OP(BlitFramebufferCHROMIUM)                /* 404 */ \
  OP(RenderbufferStorageMultisampleCHROMIUM) /* 405 */ \
  OP(RenderbufferStorageMultisampleEXT)      /* 406 */ \
  OP(FramebufferTexture2DMultisampleEXT)     /* 407 */ \
  OP(TexStorage2DEXT)                        /* 408 */ \
  OP(GenQueriesEXTImmediate)                 /* 409 */ \
  OP(DeleteQueriesEXTImmediate)              /* 410 */ \
  OP(BeginQueryEXT)                          /* 411 */ \
  OP(EndQueryEXT)                            /* 412 */ \
  OP(InsertEventMarkerEXT)                   /* 413 */ \
  OP(PushGroupMarkerEXT)                     /* 414 */ \
  OP(PopGroupMarkerEXT)                      /* 415 */ \
  OP(GenVertexArraysOESImmediate)            /* 416 */ \
  OP(DeleteVertexArraysOESImmediate)         /* 417 */ \
  OP(IsVertexArrayOES)                       /* 418 */ \
  OP(BindVertexArrayOES)                     /* 419 */ \
  OP(SwapBuffers)                            /* 420 */ \
  OP(GetMaxValueInBufferCHROMIUM)            /* 421 */ \
  OP(GenSharedIdsCHROMIUM)                   /* 422 */ \
  OP(DeleteSharedIdsCHROMIUM)                /* 423 */ \
  OP(RegisterSharedIdsCHROMIUM)              /* 424 */ \
  OP(EnableFeatureCHROMIUM)                  /* 425 */ \
  OP(ResizeCHROMIUM)                         /* 426 */ \
  OP(GetRequestableExtensionsCHROMIUM)       /* 427 */ \
  OP(RequestExtensionCHROMIUM)               /* 428 */ \
  OP(GetMultipleIntegervCHROMIUM)            /* 429 */ \
  OP(GetProgramInfoCHROMIUM)                 /* 430 */ \
  OP(GetTranslatedShaderSourceANGLE)         /* 431 */ \
  OP(PostSubBufferCHROMIUM)                  /* 432 */ \
  OP(TexImageIOSurface2DCHROMIUM)            /* 433 */ \
  OP(CopyTextureCHROMIUM)                    /* 434 */ \
  OP(DrawArraysInstancedANGLE)               /* 435 */ \
  OP(DrawElementsInstancedANGLE)             /* 436 */ \
  OP(VertexAttribDivisorANGLE)               /* 437 */ \
  OP(GenMailboxCHROMIUM)                     /* 438 */ \
  OP(ProduceTextureCHROMIUMImmediate)        /* 439 */ \
  OP(ConsumeTextureCHROMIUMImmediate)        /* 440 */ \
  OP(BindUniformLocationCHROMIUM)            /* 441 */ \
  OP(BindUniformLocationCHROMIUMBucket)      /* 442 */ \
  OP(BindTexImage2DCHROMIUM)                 /* 443 */ \
  OP(ReleaseTexImage2DCHROMIUM)              /* 444 */ \
  OP(TraceBeginCHROMIUM)                     /* 445 */ \
  OP(TraceEndCHROMIUM)                       /* 446 */ \
  OP(AsyncTexSubImage2DCHROMIUM)             /* 447 */ \
  OP(AsyncTexImage2DCHROMIUM)                /* 448 */ \
  OP(WaitAsyncTexImage2DCHROMIUM)            /* 449 */ \
  OP(WaitAllAsyncTexImage2DCHROMIUM)         /* 450 */ \
  OP(DiscardFramebufferEXTImmediate)         /* 451 */ \
  OP(LoseContextCHROMIUM)                    /* 452 */ \
  OP(InsertSyncPointCHROMIUM)                /* 453 */ \
  OP(WaitSyncPointCHROMIUM)                  /* 454 */ \
  OP(DrawBuffersEXTImmediate)                /* 455 */ \
  OP(DiscardBackbufferCHROMIUM)              /* 456 */ \
  OP(ScheduleOverlayPlaneCHROMIUM)           /* 457 */

enum CommandId {
  kStartPoint = cmd::kLastCommonId,  // All GLES2 commands start after this.
#define GLES2_CMD_OP(name) k##name,
  GLES2_COMMAND_LIST(GLES2_CMD_OP)
#undef GLES2_CMD_OP
  kNumCommands
};

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_IDS_AUTOGEN_H_
