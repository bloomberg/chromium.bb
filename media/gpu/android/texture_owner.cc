// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/texture_owner.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "media/gpu/android/image_reader_gl_owner.h"
#include "media/gpu/android/surface_texture_gl_owner.h"
#include "ui/gl/scoped_binders.h"

namespace media {

TextureOwner::TextureOwner(bool binds_texture_on_update,
                           std::unique_ptr<gpu::gles2::AbstractTexture> texture)
    : base::RefCountedDeleteOnSequence<TextureOwner>(
          base::ThreadTaskRunnerHandle::Get()),
      binds_texture_on_update_(binds_texture_on_update),
      texture_(std::move(texture)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  // Notify the subclass when the texture is destroyed.
  // Unretained is safe, since we insist that |texture_| is dropped before we're
  // destroyed, which implies that the callback has run.
  texture_->SetCleanupCallback(base::BindOnce(&TextureOwner::OnTextureDestroyed,
                                              base::Unretained(this)));
}

TextureOwner::~TextureOwner() {
  // The subclass must delete the texture before now.
  DCHECK(!texture_);
}

// static
scoped_refptr<TextureOwner> TextureOwner::Create(
    std::unique_ptr<gpu::gles2::AbstractTexture> texture,
    Mode mode) {
  // Set the parameters on the texture.
  texture->SetParameteri(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  texture->SetParameteri(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  texture->SetParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  texture->SetParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  switch (mode) {
    case Mode::kAImageReaderSecure:
      return new ImageReaderGLOwner(std::move(texture), mode);
    case Mode::kAImageReaderInsecure:
      return new ImageReaderGLOwner(std::move(texture), mode);
    case Mode::kSurfaceTextureInsecure:
      return new SurfaceTextureGLOwner(std::move(texture));
  }

  NOTREACHED();
  return nullptr;
}

// static
std::unique_ptr<gpu::gles2::AbstractTexture> TextureOwner::CreateTexture(
    gpu::DecoderContext* decoder) {
  // Note that the size isn't really used.  We just care about the service id.
  return decoder->CreateAbstractTexture(GL_TEXTURE_EXTERNAL_OES, GL_RGBA,
                                        0,  // width
                                        0,  // height
                                        1,  // depth
                                        0,  // border
                                        GL_RGBA, GL_UNSIGNED_BYTE);
}

GLuint TextureOwner::GetTextureId() const {
  return texture_->service_id();
}

void TextureOwner::ClearAbstractTexture() {
  texture_.reset();
}

}  // namespace media
