// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/video_layer_impl.h"

#include "base/logging.h"
#include "cc/io_surface_draw_quad.h"
#include "cc/layer_tree_impl.h"
#include "cc/math_util.h"
#include "cc/quad_sink.h"
#include "cc/renderer.h"
#include "cc/resource_provider.h"
#include "cc/stream_video_draw_quad.h"
#include "cc/texture_draw_quad.h"
#include "cc/video_frame_provider_client_impl.h"
#include "cc/yuv_video_draw_quad.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/filters/skcanvas_video_renderer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace cc {

// static
scoped_ptr<VideoLayerImpl> VideoLayerImpl::create(LayerTreeImpl* treeImpl, int id, VideoFrameProvider* provider)
{
    scoped_ptr<VideoLayerImpl> layer(new VideoLayerImpl(treeImpl, id));
    layer->setProviderClientImpl(VideoFrameProviderClientImpl::Create(provider));
    DCHECK(treeImpl->proxy()->isImplThread());
    DCHECK(treeImpl->proxy()->isMainThreadBlocked());
    return layer.Pass();
}

VideoLayerImpl::VideoLayerImpl(LayerTreeImpl* treeImpl, int id)
    : LayerImpl(treeImpl, id)
    , m_frame(0)
    , m_format(GL_INVALID_VALUE)
    , m_convertYUV(false)
    , m_externalTextureResource(0)
{
}

VideoLayerImpl::~VideoLayerImpl()
{
    if (!m_providerClientImpl->Stopped()) {
        // In impl side painting, we may have a pending and active layer
        // associated with the video provider at the same time. Both have a ref
        // on the VideoFrameProviderClientImpl, but we stop when the first
        // LayerImpl (the one on the pending tree) is destroyed since we know
        // the main thread is blocked for this commit.
        DCHECK(layerTreeImpl()->proxy()->isImplThread());
        DCHECK(layerTreeImpl()->proxy()->isMainThreadBlocked());
        m_providerClientImpl->Stop();
    }
    freePlaneData(layerTreeImpl()->resource_provider());

#ifndef NDEBUG
    for (size_t i = 0; i < media::VideoFrame::kMaxPlanes; ++i)
        DCHECK(!m_framePlanes[i].resourceId);
    DCHECK(!m_externalTextureResource);
#endif
}

scoped_ptr<LayerImpl> VideoLayerImpl::createLayerImpl(LayerTreeImpl* treeImpl)
{
    return scoped_ptr<LayerImpl>(new VideoLayerImpl(treeImpl, id()));
}

void VideoLayerImpl::pushPropertiesTo(LayerImpl* layer)
{
    LayerImpl::pushPropertiesTo(layer);

    VideoLayerImpl* other = static_cast<VideoLayerImpl*>(layer);
    other->setProviderClientImpl(m_providerClientImpl);
}

void VideoLayerImpl::didBecomeActive()
{
    m_providerClientImpl->set_active_video_layer(this);
}

// Convert media::VideoFrame::Format to OpenGL enum values.
static GLenum convertVFCFormatToGLenum(const media::VideoFrame& frame)
{
    switch (frame.format()) {
    case media::VideoFrame::YV12:
    case media::VideoFrame::YV16:
        return GL_LUMINANCE;
    case media::VideoFrame::NATIVE_TEXTURE:
        return frame.texture_target();
    case media::VideoFrame::INVALID:
    case media::VideoFrame::RGB32:
    case media::VideoFrame::EMPTY:
    case media::VideoFrame::I420:
        NOTREACHED();
        break;
    }
    return GL_INVALID_VALUE;
}

size_t VideoLayerImpl::numPlanes() const
{
    if (!m_frame)
        return 0;

    if (m_convertYUV)
        return 1;

    switch (m_frame->format()) {
    case media::VideoFrame::RGB32:
        return 1;
    case media::VideoFrame::YV12:
    case media::VideoFrame::YV16:
        return 3;
    case media::VideoFrame::INVALID:
    case media::VideoFrame::EMPTY:
    case media::VideoFrame::I420:
        break;
    case media::VideoFrame::NATIVE_TEXTURE:
        return 0;
    }
    NOTREACHED();
    return 0;
}

void VideoLayerImpl::willDraw(ResourceProvider* resourceProvider)
{
    LayerImpl::willDraw(resourceProvider);


    // Explicitly acquire and release the provider mutex so it can be held from
    // willDraw to didDraw. Since the compositor thread is in the middle of
    // drawing, the layer will not be destroyed before didDraw is called.
    // Therefore, the only thing that will prevent this lock from being released
    // is the GPU process locking it. As the GPU process can't cause the
    // destruction of the provider (calling stopUsingProvider), holding this
    // lock should not cause a deadlock.
    m_frame = m_providerClientImpl->AcquireLockAndCurrentFrame();

    willDrawInternal(resourceProvider);
    freeUnusedPlaneData(resourceProvider);

    if (!m_frame)
        m_providerClientImpl->ReleaseLock();
}

void VideoLayerImpl::willDrawInternal(ResourceProvider* resourceProvider)
{
    DCHECK(!m_externalTextureResource);

    if (!m_frame)
        return;

    m_format = convertVFCFormatToGLenum(*m_frame);

    // If these fail, we'll have to add draw logic that handles offset bitmap/
    // texture UVs.  For now, just expect (0, 0) offset, since all our decoders
    // so far don't offset.
    DCHECK_EQ(m_frame->visible_rect().x(), 0);
    DCHECK_EQ(m_frame->visible_rect().y(), 0);

    if (m_format == GL_INVALID_VALUE) {
        m_providerClientImpl->PutCurrentFrame(m_frame);
        m_frame = 0;
        return;
    }

    // FIXME: If we're in software compositing mode, we do the YUV -> RGB
    // conversion here. That involves an extra copy of each frame to a bitmap.
    // Obviously, this is suboptimal and should be addressed once ubercompositor
    // starts shaping up.
    m_convertYUV = resourceProvider->defaultResourceType() == ResourceProvider::Bitmap &&
        (m_frame->format() == media::VideoFrame::YV12 ||
         m_frame->format() == media::VideoFrame::YV16);

    if (m_convertYUV)
        m_format = GL_RGBA;

    if (!allocatePlaneData(resourceProvider)) {
        m_providerClientImpl->PutCurrentFrame(m_frame);
        m_frame = 0;
        return;
    }

    if (!copyPlaneData(resourceProvider)) {
        m_providerClientImpl->PutCurrentFrame(m_frame);
        m_frame = 0;
        return;
    }

    if (m_format == GL_TEXTURE_2D)
        m_externalTextureResource = resourceProvider->createResourceFromExternalTexture(m_frame->texture_id());
}

void VideoLayerImpl::appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData)
{
    if (!m_frame)
        return;

    SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState, appendQuadsData);

    // FIXME: When we pass quads out of process, we need to double-buffer, or
    // otherwise synchonize use of all textures in the quad.

    gfx::Rect quadRect(gfx::Point(), contentBounds());
    gfx::Rect opaqueRect(contentsOpaque() ? quadRect : gfx::Rect());
    gfx::Rect visibleRect = m_frame->visible_rect();
    gfx::Size codedSize = m_frame->coded_size();

    // pixels for macroblocked formats.
    const float texWidthScale =
        static_cast<float>(visibleRect.width()) / codedSize.width();
    const float texHeightScale =
        static_cast<float>(visibleRect.height()) / codedSize.height();

    switch (m_format) {
    case GL_LUMINANCE: {
        // YUV software decoder.
        const FramePlane& yPlane = m_framePlanes[media::VideoFrame::kYPlane];
        const FramePlane& uPlane = m_framePlanes[media::VideoFrame::kUPlane];
        const FramePlane& vPlane = m_framePlanes[media::VideoFrame::kVPlane];
        gfx::SizeF texScale(texWidthScale, texHeightScale);
        scoped_ptr<YUVVideoDrawQuad> yuvVideoQuad = YUVVideoDrawQuad::Create();
        yuvVideoQuad->SetNew(sharedQuadState, quadRect, opaqueRect, texScale, yPlane, uPlane, vPlane);
        quadSink.append(yuvVideoQuad.PassAs<DrawQuad>(), appendQuadsData);
        break;
    }
    case GL_RGBA: {
        // RGBA software decoder.
        const FramePlane& plane = m_framePlanes[media::VideoFrame::kRGBPlane];
        bool premultipliedAlpha = true;
        gfx::PointF uvTopLeft(0.f, 0.f);
        gfx::PointF uvBottomRight(texWidthScale, texHeightScale);
        const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
        bool flipped = false;
        scoped_ptr<TextureDrawQuad> textureQuad = TextureDrawQuad::Create();
        textureQuad->SetNew(sharedQuadState, quadRect, opaqueRect, plane.resourceId, premultipliedAlpha, uvTopLeft, uvBottomRight, opacity, flipped);
        quadSink.append(textureQuad.PassAs<DrawQuad>(), appendQuadsData);
        break;
    }
    case GL_TEXTURE_2D: {
        // NativeTexture hardware decoder.
        bool premultipliedAlpha = true;
        gfx::PointF uvTopLeft(0.f, 0.f);
        gfx::PointF uvBottomRight(texWidthScale, texHeightScale);
        const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
        bool flipped = false;
        scoped_ptr<TextureDrawQuad> textureQuad = TextureDrawQuad::Create();
        textureQuad->SetNew(sharedQuadState, quadRect, opaqueRect, m_externalTextureResource, premultipliedAlpha, uvTopLeft, uvBottomRight, opacity, flipped);
        quadSink.append(textureQuad.PassAs<DrawQuad>(), appendQuadsData);
        break;
    }
    case GL_TEXTURE_RECTANGLE_ARB: {
        gfx::Size visibleSize(visibleRect.width(), visibleRect.height());
        scoped_ptr<IOSurfaceDrawQuad> ioSurfaceQuad = IOSurfaceDrawQuad::Create();
        ioSurfaceQuad->SetNew(sharedQuadState, quadRect, opaqueRect, visibleSize, m_frame->texture_id(), IOSurfaceDrawQuad::UNFLIPPED);
        quadSink.append(ioSurfaceQuad.PassAs<DrawQuad>(), appendQuadsData);
        break;
    }
    case GL_TEXTURE_EXTERNAL_OES: {
        // StreamTexture hardware decoder.
        gfx::Transform transform(m_providerClientImpl->stream_texture_matrix());
        transform.Scale(texWidthScale, texHeightScale);
        scoped_ptr<StreamVideoDrawQuad> streamVideoQuad = StreamVideoDrawQuad::Create();
        streamVideoQuad->SetNew(sharedQuadState, quadRect, opaqueRect, m_frame->texture_id(), transform);
        quadSink.append(streamVideoQuad.PassAs<DrawQuad>(), appendQuadsData);
        break;
    }
    default:
        NOTREACHED();  // Someone updated convertVFCFormatToGLenum above but update this!
        break;
    }
}

void VideoLayerImpl::didDraw(ResourceProvider* resourceProvider)
{
    LayerImpl::didDraw(resourceProvider);

    if (!m_frame)
        return;

    if (m_format == GL_TEXTURE_2D) {
        DCHECK(m_externalTextureResource);
        // FIXME: the following assert will not be true when sending resources to a
        // parent compositor. We will probably need to hold on to m_frame for
        // longer, and have several "current frames" in the pipeline.
        DCHECK(!resourceProvider->inUseByConsumer(m_externalTextureResource));
        resourceProvider->deleteResource(m_externalTextureResource);
        m_externalTextureResource = 0;
    }

    m_providerClientImpl->PutCurrentFrame(m_frame);
    m_frame = 0;

    m_providerClientImpl->ReleaseLock();
}

static gfx::Size videoFrameDimension(media::VideoFrame* frame, int plane) {
    gfx::Size dimensions = frame->coded_size();
    switch (frame->format()) {
    case media::VideoFrame::YV12:
        if (plane != media::VideoFrame::kYPlane) {
            dimensions.set_width(dimensions.width() / 2);
            dimensions.set_height(dimensions.height() / 2);
        }
        break;
    case media::VideoFrame::YV16:
        if (plane != media::VideoFrame::kYPlane) {
            dimensions.set_width(dimensions.width() / 2);
        }
        break;
    default:
        break;
    }
    return dimensions;
}

bool VideoLayerImpl::FramePlane::allocateData(
    ResourceProvider* resourceProvider)
{
    if (resourceId)
        return true;

    resourceId = resourceProvider->createResource(size, format, ResourceProvider::TextureUsageAny);
    return resourceId;
}

void VideoLayerImpl::FramePlane::freeData(ResourceProvider* resourceProvider)
{
    if (!resourceId)
        return;

    resourceProvider->deleteResource(resourceId);
    resourceId = 0;
}

bool VideoLayerImpl::allocatePlaneData(ResourceProvider* resourceProvider)
{
    const int maxTextureSize = resourceProvider->maxTextureSize();
    const size_t planeCount = numPlanes();
    for (unsigned planeIdx = 0; planeIdx < planeCount; ++planeIdx) {
        VideoLayerImpl::FramePlane& plane = m_framePlanes[planeIdx];

        gfx::Size requiredTextureSize = videoFrameDimension(m_frame, planeIdx);
        // FIXME: Remove the test against maxTextureSize when tiled layers are
        // implemented.
        if (requiredTextureSize.IsEmpty() ||
            requiredTextureSize.width() > maxTextureSize ||
            requiredTextureSize.height() > maxTextureSize)
            return false;

        if (plane.size != requiredTextureSize || plane.format != m_format) {
            plane.freeData(resourceProvider);
            plane.size = requiredTextureSize;
            plane.format = m_format;
        }

        if (!plane.allocateData(resourceProvider))
            return false;
    }
    return true;
}

bool VideoLayerImpl::copyPlaneData(ResourceProvider* resourceProvider)
{
    const size_t planeCount = numPlanes();
    if (!planeCount)
        return true;

    if (m_convertYUV) {
        if (!m_videoRenderer)
            m_videoRenderer.reset(new media::SkCanvasVideoRenderer);
        VideoLayerImpl::FramePlane& plane = m_framePlanes[media::VideoFrame::kRGBPlane];
        ResourceProvider::ScopedWriteLockSoftware lock(resourceProvider, plane.resourceId);
        m_videoRenderer->Paint(m_frame, lock.skCanvas(), m_frame->visible_rect(), 0xFF);
        return true;
    }

    for (size_t planeIndex = 0; planeIndex < planeCount; ++planeIndex) {
        VideoLayerImpl::FramePlane& plane = m_framePlanes[planeIndex];
        // Only non-FormatNativeTexture planes should need upload.
        DCHECK_EQ(plane.format, GL_LUMINANCE);
        const uint8_t* softwarePlanePixels = m_frame->data(planeIndex);
        gfx::Rect imageRect(0, 0, m_frame->stride(planeIndex), plane.size.height());
        gfx::Rect sourceRect(gfx::Point(), plane.size);
        resourceProvider->setPixels(plane.resourceId, softwarePlanePixels, imageRect, sourceRect, gfx::Vector2d());
    }
    return true;
}

void VideoLayerImpl::freePlaneData(ResourceProvider* resourceProvider)
{
    for (size_t i = 0; i < media::VideoFrame::kMaxPlanes; ++i)
        m_framePlanes[i].freeData(resourceProvider);
}

void VideoLayerImpl::freeUnusedPlaneData(ResourceProvider* resourceProvider)
{
    size_t firstUnusedPlane = numPlanes();
    for (size_t i = firstUnusedPlane; i < media::VideoFrame::kMaxPlanes; ++i)
        m_framePlanes[i].freeData(resourceProvider);
}

void VideoLayerImpl::didLoseOutputSurface()
{
    freePlaneData(layerTreeImpl()->resource_provider());
}

void VideoLayerImpl::setNeedsRedraw()
{
    layerTreeImpl()->SetNeedsRedraw();
}

void VideoLayerImpl::setProviderClientImpl(scoped_refptr<VideoFrameProviderClientImpl> providerClientImpl)
{
    m_providerClientImpl = providerClientImpl;
}

const char* VideoLayerImpl::layerTypeAsString() const
{
    return "VideoLayer";
}

}  // namespace cc
