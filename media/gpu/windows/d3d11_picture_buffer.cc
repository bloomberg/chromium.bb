// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_picture_buffer.h"

#include <windows.h>

#include "media/gpu/windows/return_on_failure.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_dxgi.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"

namespace media {

D3D11PictureBuffer::D3D11PictureBuffer(PictureBuffer picture_buffer,
                                       size_t level)
    : picture_buffer_(picture_buffer), level_(level) {}

D3D11PictureBuffer::D3D11PictureBuffer(
    PictureBuffer picture_buffer,
    size_t level,
    const std::vector<scoped_refptr<gpu::gles2::TextureRef>>& texture_refs,
    const MailboxHolderArray& mailbox_holders)
    : picture_buffer_(picture_buffer),
      level_(level),
      texture_refs_(texture_refs) {
  memcpy(&mailbox_holders_, mailbox_holders, sizeof(mailbox_holders_));
}

D3D11PictureBuffer::~D3D11PictureBuffer() {}

bool D3D11PictureBuffer::Init(
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
    const GUID& decoder_guid) {
  texture_ = texture;
  D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC view_desc = {};
  view_desc.DecodeProfile = decoder_guid;
  view_desc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
  view_desc.Texture2D.ArraySlice = (UINT)level_;

  HRESULT hr = video_device->CreateVideoDecoderOutputView(
      texture.Get(), &view_desc, output_view_.GetAddressOf());

  CHECK(SUCCEEDED(hr));
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  const EGLint stream_attributes[] = {
      EGL_CONSUMER_LATENCY_USEC_KHR,
      0,
      EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR,
      0,
      EGL_NONE,
  };
  stream_ = eglCreateStreamKHR(egl_display, stream_attributes);
  RETURN_ON_FAILURE(!!stream_, "Could not create stream", false);
  gl_image_ =
      base::MakeRefCounted<gl::GLImageDXGI>(picture_buffer_.size(), stream_);
  gl::ScopedActiveTexture texture0(GL_TEXTURE0);
  gl::ScopedTextureBinder texture0_binder(
      GL_TEXTURE_EXTERNAL_OES, picture_buffer_.service_texture_ids()[0]);
  gl::ScopedActiveTexture texture1(GL_TEXTURE1);
  gl::ScopedTextureBinder texture1_binder(
      GL_TEXTURE_EXTERNAL_OES, picture_buffer_.service_texture_ids()[1]);

  EGLAttrib consumer_attributes[] = {
      EGL_COLOR_BUFFER_TYPE,
      EGL_YUV_BUFFER_EXT,
      EGL_YUV_NUMBER_OF_PLANES_EXT,
      2,
      EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
      0,
      EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
      1,
      EGL_NONE,
  };
  EGLBoolean result = eglStreamConsumerGLTextureExternalAttribsNV(
      egl_display, stream_, consumer_attributes);
  RETURN_ON_FAILURE(result, "Could not set stream consumer", false);

  EGLAttrib producer_attributes[] = {
      EGL_NONE,
  };

  result = eglCreateStreamProducerD3DTextureANGLE(egl_display, stream_,
                                                  producer_attributes);

  EGLAttrib frame_attributes[] = {
      EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, level_, EGL_NONE,
  };

  result = eglStreamPostD3DTextureANGLE(egl_display, stream_,
                                        static_cast<void*>(texture.Get()),
                                        frame_attributes);
  RETURN_ON_FAILURE(result, "Could not post texture", false);
  result = eglStreamConsumerAcquireKHR(egl_display, stream_);
  RETURN_ON_FAILURE(result, "Could not post acquire stream", false);
  gl::GLImageDXGI* gl_image_dxgi =
      static_cast<gl::GLImageDXGI*>(gl_image_.get());

  gl_image_dxgi->SetTexture(texture, level_);
  return true;
}

}  // namespace media
