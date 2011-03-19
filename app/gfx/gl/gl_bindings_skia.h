// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_GL_GL_BINDINGS_SKIA_H_
#define APP_GFX_GL_GL_BINDINGS_SKIA_H_
#pragma once

namespace gfx {

// The GPU back-end for skia normally makes direct calls to GLES2\gl.  Call
// this routine to bind the skia GL implementation to that exported by the
// app/gfx/gl entry points.
// Note:  This override is required to allow the skia gl calls to be correctly
// routed around the command buffers during execution of the DumpRenderTree
// layout tests.
void BindSkiaToHostGL();

}

#endif  // APP_GFX_GL_GL_BINDINGS_SKIA_H_
