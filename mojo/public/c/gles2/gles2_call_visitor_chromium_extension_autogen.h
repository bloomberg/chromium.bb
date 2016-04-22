// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

VISIT_GL_CALL(ShallowFinishCHROMIUM, void, (), ())
VISIT_GL_CALL(ShallowFlushCHROMIUM, void, (), ())
VISIT_GL_CALL(OrderingBarrierCHROMIUM, void, (), ())
VISIT_GL_CALL(
    BlitFramebufferCHROMIUM,
    void,
    (GLint srcX0,
     GLint srcY0,
     GLint srcX1,
     GLint srcY1,
     GLint dstX0,
     GLint dstY0,
     GLint dstX1,
     GLint dstY1,
     GLbitfield mask,
     GLenum filter),
    (srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter))
VISIT_GL_CALL(RenderbufferStorageMultisampleCHROMIUM,
              void,
              (GLenum target,
               GLsizei samples,
               GLenum internalformat,
               GLsizei width,
               GLsizei height),
              (target, samples, internalformat, width, height))
VISIT_GL_CALL(RenderbufferStorageMultisampleEXT,
              void,
              (GLenum target,
               GLsizei samples,
               GLenum internalformat,
               GLsizei width,
               GLsizei height),
              (target, samples, internalformat, width, height))
VISIT_GL_CALL(FramebufferTexture2DMultisampleEXT,
              void,
              (GLenum target,
               GLenum attachment,
               GLenum textarget,
               GLuint texture,
               GLint level,
               GLsizei samples),
              (target, attachment, textarget, texture, level, samples))
VISIT_GL_CALL(TexStorage2DEXT,
              void,
              (GLenum target,
               GLsizei levels,
               GLenum internalFormat,
               GLsizei width,
               GLsizei height),
              (target, levels, internalFormat, width, height))
VISIT_GL_CALL(GenQueriesEXT, void, (GLsizei n, GLuint* queries), (n, queries))
VISIT_GL_CALL(DeleteQueriesEXT,
              void,
              (GLsizei n, const GLuint* queries),
              (n, queries))
VISIT_GL_CALL(QueryCounterEXT, void, (GLuint id, GLenum target), (id, target))
VISIT_GL_CALL(IsQueryEXT, GLboolean, (GLuint id), (id))
VISIT_GL_CALL(BeginQueryEXT, void, (GLenum target, GLuint id), (target, id))
VISIT_GL_CALL(EndQueryEXT, void, (GLenum target), (target))
VISIT_GL_CALL(GetQueryivEXT,
              void,
              (GLenum target, GLenum pname, GLint* params),
              (target, pname, params))
VISIT_GL_CALL(GetQueryObjectivEXT,
              void,
              (GLuint id, GLenum pname, GLint* params),
              (id, pname, params))
VISIT_GL_CALL(GetQueryObjectuivEXT,
              void,
              (GLuint id, GLenum pname, GLuint* params),
              (id, pname, params))
VISIT_GL_CALL(GetQueryObjecti64vEXT,
              void,
              (GLuint id, GLenum pname, GLint64* params),
              (id, pname, params))
VISIT_GL_CALL(GetQueryObjectui64vEXT,
              void,
              (GLuint id, GLenum pname, GLuint64* params),
              (id, pname, params))
VISIT_GL_CALL(SetDisjointValueSyncCHROMIUM, void, (), ())
VISIT_GL_CALL(InsertEventMarkerEXT,
              void,
              (GLsizei length, const GLchar* marker),
              (length, marker))
VISIT_GL_CALL(PushGroupMarkerEXT,
              void,
              (GLsizei length, const GLchar* marker),
              (length, marker))
VISIT_GL_CALL(PopGroupMarkerEXT, void, (), ())
VISIT_GL_CALL(GenVertexArraysOES,
              void,
              (GLsizei n, GLuint* arrays),
              (n, arrays))
VISIT_GL_CALL(DeleteVertexArraysOES,
              void,
              (GLsizei n, const GLuint* arrays),
              (n, arrays))
VISIT_GL_CALL(IsVertexArrayOES, GLboolean, (GLuint array), (array))
VISIT_GL_CALL(BindVertexArrayOES, void, (GLuint array), (array))
VISIT_GL_CALL(SwapBuffers, void, (), ())
VISIT_GL_CALL(GetMaxValueInBufferCHROMIUM,
              GLuint,
              (GLuint buffer_id, GLsizei count, GLenum type, GLuint offset),
              (buffer_id, count, type, offset))
VISIT_GL_CALL(EnableFeatureCHROMIUM,
              GLboolean,
              (const char* feature),
              (feature))
VISIT_GL_CALL(MapBufferCHROMIUM,
              void*,
              (GLuint target, GLenum access),
              (target, access))
VISIT_GL_CALL(UnmapBufferCHROMIUM, GLboolean, (GLuint target), (target))
VISIT_GL_CALL(MapBufferSubDataCHROMIUM,
              void*,
              (GLuint target, GLintptr offset, GLsizeiptr size, GLenum access),
              (target, offset, size, access))
VISIT_GL_CALL(UnmapBufferSubDataCHROMIUM, void, (const void* mem), (mem))
VISIT_GL_CALL(
    MapTexSubImage2DCHROMIUM,
    void*,
    (GLenum target,
     GLint level,
     GLint xoffset,
     GLint yoffset,
     GLsizei width,
     GLsizei height,
     GLenum format,
     GLenum type,
     GLenum access),
    (target, level, xoffset, yoffset, width, height, format, type, access))
VISIT_GL_CALL(UnmapTexSubImage2DCHROMIUM, void, (const void* mem), (mem))
VISIT_GL_CALL(
    ResizeCHROMIUM,
    void,
    (GLuint width, GLuint height, GLfloat scale_factor, GLboolean alpha),
    (width, height, scale_factor, alpha))
VISIT_GL_CALL(GetRequestableExtensionsCHROMIUM, const GLchar*, (), ())
VISIT_GL_CALL(RequestExtensionCHROMIUM,
              void,
              (const char* extension),
              (extension))
VISIT_GL_CALL(GetProgramInfoCHROMIUM,
              void,
              (GLuint program, GLsizei bufsize, GLsizei* size, void* info),
              (program, bufsize, size, info))
VISIT_GL_CALL(
    CreateImageCHROMIUM,
    GLuint,
    (ClientBuffer buffer, GLsizei width, GLsizei height, GLenum internalformat),
    (buffer, width, height, internalformat))
VISIT_GL_CALL(DestroyImageCHROMIUM, void, (GLuint image_id), (image_id))
VISIT_GL_CALL(
    CreateGpuMemoryBufferImageCHROMIUM,
    GLuint,
    (GLsizei width, GLsizei height, GLenum internalformat, GLenum usage),
    (width, height, internalformat, usage))
VISIT_GL_CALL(GetTranslatedShaderSourceANGLE,
              void,
              (GLuint shader, GLsizei bufsize, GLsizei* length, char* source),
              (shader, bufsize, length, source))
VISIT_GL_CALL(PostSubBufferCHROMIUM,
              void,
              (GLint x, GLint y, GLint width, GLint height),
              (x, y, width, height))
VISIT_GL_CALL(TexImageIOSurface2DCHROMIUM,
              void,
              (GLenum target,
               GLsizei width,
               GLsizei height,
               GLuint ioSurfaceId,
               GLuint plane),
              (target, width, height, ioSurfaceId, plane))
VISIT_GL_CALL(CopyTextureCHROMIUM,
              void,
              (GLenum source_id,
               GLenum dest_id,
               GLint internalformat,
               GLenum dest_type,
               GLboolean unpack_flip_y,
               GLboolean unpack_premultiply_alpha,
               GLboolean unpack_unmultiply_alpha),
              (source_id,
               dest_id,
               internalformat,
               dest_type,
               unpack_flip_y,
               unpack_premultiply_alpha,
               unpack_unmultiply_alpha))
VISIT_GL_CALL(CopySubTextureCHROMIUM,
              void,
              (GLenum source_id,
               GLenum dest_id,
               GLint xoffset,
               GLint yoffset,
               GLint x,
               GLint y,
               GLsizei width,
               GLsizei height,
               GLboolean unpack_flip_y,
               GLboolean unpack_premultiply_alpha,
               GLboolean unpack_unmultiply_alpha),
              (source_id,
               dest_id,
               xoffset,
               yoffset,
               x,
               y,
               width,
               height,
               unpack_flip_y,
               unpack_premultiply_alpha,
               unpack_unmultiply_alpha))
VISIT_GL_CALL(CompressedCopyTextureCHROMIUM,
              void,
              (GLenum source_id, GLenum dest_id),
              (source_id, dest_id))
VISIT_GL_CALL(DrawArraysInstancedANGLE,
              void,
              (GLenum mode, GLint first, GLsizei count, GLsizei primcount),
              (mode, first, count, primcount))
VISIT_GL_CALL(DrawElementsInstancedANGLE,
              void,
              (GLenum mode,
               GLsizei count,
               GLenum type,
               const void* indices,
               GLsizei primcount),
              (mode, count, type, indices, primcount))
VISIT_GL_CALL(VertexAttribDivisorANGLE,
              void,
              (GLuint index, GLuint divisor),
              (index, divisor))
VISIT_GL_CALL(GenMailboxCHROMIUM, void, (GLbyte * mailbox), (mailbox))
VISIT_GL_CALL(ProduceTextureCHROMIUM,
              void,
              (GLenum target, const GLbyte* mailbox),
              (target, mailbox))
VISIT_GL_CALL(ProduceTextureDirectCHROMIUM,
              void,
              (GLuint texture, GLenum target, const GLbyte* mailbox),
              (texture, target, mailbox))
VISIT_GL_CALL(ConsumeTextureCHROMIUM,
              void,
              (GLenum target, const GLbyte* mailbox),
              (target, mailbox))
VISIT_GL_CALL(CreateAndConsumeTextureCHROMIUM,
              GLuint,
              (GLenum target, const GLbyte* mailbox),
              (target, mailbox))
VISIT_GL_CALL(BindUniformLocationCHROMIUM,
              void,
              (GLuint program, GLint location, const char* name),
              (program, location, name))
VISIT_GL_CALL(BindTexImage2DCHROMIUM,
              void,
              (GLenum target, GLint imageId),
              (target, imageId))
VISIT_GL_CALL(ReleaseTexImage2DCHROMIUM,
              void,
              (GLenum target, GLint imageId),
              (target, imageId))
VISIT_GL_CALL(TraceBeginCHROMIUM,
              void,
              (const char* category_name, const char* trace_name),
              (category_name, trace_name))
VISIT_GL_CALL(TraceEndCHROMIUM, void, (), ())
VISIT_GL_CALL(DiscardFramebufferEXT,
              void,
              (GLenum target, GLsizei count, const GLenum* attachments),
              (target, count, attachments))
VISIT_GL_CALL(LoseContextCHROMIUM,
              void,
              (GLenum current, GLenum other),
              (current, other))
VISIT_GL_CALL(InsertFenceSyncCHROMIUM, GLuint64, (), ())
VISIT_GL_CALL(GenSyncTokenCHROMIUM,
              void,
              (GLuint64 fence_sync, GLbyte* sync_token),
              (fence_sync, sync_token))
VISIT_GL_CALL(GenUnverifiedSyncTokenCHROMIUM,
              void,
              (GLuint64 fence_sync, GLbyte* sync_token),
              (fence_sync, sync_token))
VISIT_GL_CALL(VerifySyncTokensCHROMIUM,
              void,
              (GLbyte * *sync_tokens, GLsizei count),
              (sync_tokens, count))
VISIT_GL_CALL(WaitSyncTokenCHROMIUM,
              void,
              (const GLbyte* sync_token),
              (sync_token))
VISIT_GL_CALL(DrawBuffersEXT,
              void,
              (GLsizei count, const GLenum* bufs),
              (count, bufs))
VISIT_GL_CALL(DiscardBackbufferCHROMIUM, void, (), ())
VISIT_GL_CALL(ScheduleOverlayPlaneCHROMIUM,
              void,
              (GLint plane_z_order,
               GLenum plane_transform,
               GLuint overlay_texture_id,
               GLint bounds_x,
               GLint bounds_y,
               GLint bounds_width,
               GLint bounds_height,
               GLfloat uv_x,
               GLfloat uv_y,
               GLfloat uv_width,
               GLfloat uv_height),
              (plane_z_order,
               plane_transform,
               overlay_texture_id,
               bounds_x,
               bounds_y,
               bounds_width,
               bounds_height,
               uv_x,
               uv_y,
               uv_width,
               uv_height))
VISIT_GL_CALL(ScheduleCALayerCHROMIUM,
              void,
              (GLuint contents_texture_id,
               const GLfloat* contents_rect,
               GLfloat opacity,
               GLuint background_color,
               GLuint edge_aa_mask,
               const GLfloat* bounds_rect,
               GLboolean is_clipped,
               const GLfloat* clip_rect,
               GLint sorting_context_id,
               const GLfloat* transform,
               GLuint filter),
              (contents_texture_id,
               contents_rect,
               opacity,
               background_color,
               edge_aa_mask,
               bounds_rect,
               is_clipped,
               clip_rect,
               sorting_context_id,
               transform,
               filter))
VISIT_GL_CALL(CommitOverlayPlanesCHROMIUM, void, (), ())
VISIT_GL_CALL(SwapInterval, void, (GLint interval), (interval))
VISIT_GL_CALL(FlushDriverCachesCHROMIUM, void, (), ())
VISIT_GL_CALL(GetLastFlushIdCHROMIUM, GLuint, (), ())
VISIT_GL_CALL(MatrixLoadfCHROMIUM,
              void,
              (GLenum matrixMode, const GLfloat* m),
              (matrixMode, m))
VISIT_GL_CALL(MatrixLoadIdentityCHROMIUM,
              void,
              (GLenum matrixMode),
              (matrixMode))
VISIT_GL_CALL(GenPathsCHROMIUM, GLuint, (GLsizei range), (range))
VISIT_GL_CALL(DeletePathsCHROMIUM,
              void,
              (GLuint path, GLsizei range),
              (path, range))
VISIT_GL_CALL(IsPathCHROMIUM, GLboolean, (GLuint path), (path))
VISIT_GL_CALL(PathCommandsCHROMIUM,
              void,
              (GLuint path,
               GLsizei numCommands,
               const GLubyte* commands,
               GLsizei numCoords,
               GLenum coordType,
               const GLvoid* coords),
              (path, numCommands, commands, numCoords, coordType, coords))
VISIT_GL_CALL(PathParameterfCHROMIUM,
              void,
              (GLuint path, GLenum pname, GLfloat value),
              (path, pname, value))
VISIT_GL_CALL(PathParameteriCHROMIUM,
              void,
              (GLuint path, GLenum pname, GLint value),
              (path, pname, value))
VISIT_GL_CALL(PathStencilFuncCHROMIUM,
              void,
              (GLenum func, GLint ref, GLuint mask),
              (func, ref, mask))
VISIT_GL_CALL(StencilFillPathCHROMIUM,
              void,
              (GLuint path, GLenum fillMode, GLuint mask),
              (path, fillMode, mask))
VISIT_GL_CALL(StencilStrokePathCHROMIUM,
              void,
              (GLuint path, GLint reference, GLuint mask),
              (path, reference, mask))
VISIT_GL_CALL(CoverFillPathCHROMIUM,
              void,
              (GLuint path, GLenum coverMode),
              (path, coverMode))
VISIT_GL_CALL(CoverStrokePathCHROMIUM,
              void,
              (GLuint path, GLenum coverMode),
              (path, coverMode))
VISIT_GL_CALL(StencilThenCoverFillPathCHROMIUM,
              void,
              (GLuint path, GLenum fillMode, GLuint mask, GLenum coverMode),
              (path, fillMode, mask, coverMode))
VISIT_GL_CALL(StencilThenCoverStrokePathCHROMIUM,
              void,
              (GLuint path, GLint reference, GLuint mask, GLenum coverMode),
              (path, reference, mask, coverMode))
VISIT_GL_CALL(StencilFillPathInstancedCHROMIUM,
              void,
              (GLsizei numPaths,
               GLenum pathNameType,
               const GLvoid* paths,
               GLuint pathBase,
               GLenum fillMode,
               GLuint mask,
               GLenum transformType,
               const GLfloat* transformValues),
              (numPaths,
               pathNameType,
               paths,
               pathBase,
               fillMode,
               mask,
               transformType,
               transformValues))
VISIT_GL_CALL(StencilStrokePathInstancedCHROMIUM,
              void,
              (GLsizei numPaths,
               GLenum pathNameType,
               const GLvoid* paths,
               GLuint pathBase,
               GLint reference,
               GLuint mask,
               GLenum transformType,
               const GLfloat* transformValues),
              (numPaths,
               pathNameType,
               paths,
               pathBase,
               reference,
               mask,
               transformType,
               transformValues))
VISIT_GL_CALL(CoverFillPathInstancedCHROMIUM,
              void,
              (GLsizei numPaths,
               GLenum pathNameType,
               const GLvoid* paths,
               GLuint pathBase,
               GLenum coverMode,
               GLenum transformType,
               const GLfloat* transformValues),
              (numPaths,
               pathNameType,
               paths,
               pathBase,
               coverMode,
               transformType,
               transformValues))
VISIT_GL_CALL(CoverStrokePathInstancedCHROMIUM,
              void,
              (GLsizei numPaths,
               GLenum pathNameType,
               const GLvoid* paths,
               GLuint pathBase,
               GLenum coverMode,
               GLenum transformType,
               const GLfloat* transformValues),
              (numPaths,
               pathNameType,
               paths,
               pathBase,
               coverMode,
               transformType,
               transformValues))
VISIT_GL_CALL(StencilThenCoverFillPathInstancedCHROMIUM,
              void,
              (GLsizei numPaths,
               GLenum pathNameType,
               const GLvoid* paths,
               GLuint pathBase,
               GLenum fillMode,
               GLuint mask,
               GLenum coverMode,
               GLenum transformType,
               const GLfloat* transformValues),
              (numPaths,
               pathNameType,
               paths,
               pathBase,
               fillMode,
               mask,
               coverMode,
               transformType,
               transformValues))
VISIT_GL_CALL(StencilThenCoverStrokePathInstancedCHROMIUM,
              void,
              (GLsizei numPaths,
               GLenum pathNameType,
               const GLvoid* paths,
               GLuint pathBase,
               GLint reference,
               GLuint mask,
               GLenum coverMode,
               GLenum transformType,
               const GLfloat* transformValues),
              (numPaths,
               pathNameType,
               paths,
               pathBase,
               reference,
               mask,
               coverMode,
               transformType,
               transformValues))
VISIT_GL_CALL(BindFragmentInputLocationCHROMIUM,
              void,
              (GLuint program, GLint location, const char* name),
              (program, location, name))
VISIT_GL_CALL(ProgramPathFragmentInputGenCHROMIUM,
              void,
              (GLuint program,
               GLint location,
               GLenum genMode,
               GLint components,
               const GLfloat* coeffs),
              (program, location, genMode, components, coeffs))
VISIT_GL_CALL(CoverageModulationCHROMIUM,
              void,
              (GLenum components),
              (components))
VISIT_GL_CALL(GetGraphicsResetStatusKHR, GLenum, (), ())
VISIT_GL_CALL(BlendBarrierKHR, void, (), ())
VISIT_GL_CALL(ApplyScreenSpaceAntialiasingCHROMIUM, void, (), ())
VISIT_GL_CALL(
    BindFragDataLocationIndexedEXT,
    void,
    (GLuint program, GLuint colorNumber, GLuint index, const char* name),
    (program, colorNumber, index, name))
VISIT_GL_CALL(BindFragDataLocationEXT,
              void,
              (GLuint program, GLuint colorNumber, const char* name),
              (program, colorNumber, name))
VISIT_GL_CALL(GetFragDataIndexEXT,
              GLint,
              (GLuint program, const char* name),
              (program, name))
VISIT_GL_CALL(UniformMatrix4fvStreamTextureMatrixCHROMIUM,
              void,
              (GLint location,
               GLboolean transpose,
               const GLfloat* default_value),
              (location, transpose, default_value))
