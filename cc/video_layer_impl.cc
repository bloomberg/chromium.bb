// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/video_layer_impl.h"

#include "base/logging.h"
#include "cc/io_surface_draw_quad.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/proxy.h"
#include "cc/quad_sink.h"
#include "cc/resource_provider.h"
#include "cc/stream_video_draw_quad.h"
#include "cc/texture_draw_quad.h"
#include "cc/yuv_video_draw_quad.h"
#include "media/filters/skcanvas_video_renderer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace cc {

VideoLayerImpl::VideoLayerImpl(int id, WebKit::WebVideoFrameProvider* provider,
                               const FrameUnwrapper& unwrapper)
    : LayerImpl(id)
    , m_provider(provider)
    , m_unwrapper(unwrapper)
    , m_webFrame(0)
    , m_frame(0)
    , m_format(GL_INVALID_VALUE)
    , m_convertYUV(false)
    , m_externalTextureResource(0)
{
    // This matrix is the default transformation for stream textures, and flips on the Y axis.
    m_streamTextureMatrix = WebKit::WebTransformationMatrix(
        1, 0, 0, 0,
        0, -1, 0, 0,
        0, 0, 1, 0,
        0, 1, 0, 1);

    // This only happens during a commit on the compositor thread while the main
    // thread is blocked. That makes this a thread-safe call to set the video
    // frame provider client that does not require a lock. The same is true of
    // the call in the destructor.
    DCHECK(Proxy::isMainThreadBlocked());
    m_provider->setVideoFrameProviderClient(this);
}

VideoLayerImpl::~VideoLayerImpl()
{
    // See comment in constructor for why this doesn't need a lock.
    DCHECK(Proxy::isMainThreadBlocked());
    if (m_provider) {
        m_provider->setVideoFrameProviderClient(0);
        m_provider = 0;
    }
    freePlaneData(layerTreeHostImpl()->resourceProvider());

#ifndef NDEBUG
    for (size_t i = 0; i < media::VideoFrame::kMaxPlanes; ++i)
        DCHECK(!m_framePlanes[i].resourceId);
    DCHECK(!m_externalTextureResource);
#endif
}

void VideoLayerImpl::stopUsingProvider()
{
    // Block the provider from shutting down until this client is done
    // using the frame.
    base::AutoLock locker(m_providerLock);
    DCHECK(!m_frame);
    m_provider = 0;
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
    DCHECK(Proxy::isImplThread());
    LayerImpl::willDraw(resourceProvider);

    // Explicitly acquire and release the provider mutex so it can be held from
    // willDraw to didDraw. Since the compositor thread is in the middle of
    // drawing, the layer will not be destroyed before didDraw is called.
    // Therefore, the only thing that will prevent this lock from being released
    // is the GPU process locking it. As the GPU process can't cause the
    // destruction of the provider (calling stopUsingProvider), holding this
    // lock should not cause a deadlock.
    m_providerLock.Acquire();

    willDrawInternal(resourceProvider);
    freeUnusedPlaneData(resourceProvider);

    if (!m_frame)
        m_providerLock.Release();
}

void VideoLayerImpl::willDrawInternal(ResourceProvider* resourceProvider)
{
    DCHECK(Proxy::isImplThread());
    DCHECK(!m_externalTextureResource);

    if (!m_provider) {
        m_frame = 0;
        return;
    }

    m_webFrame = m_provider->getCurrentFrame();
    m_frame = m_unwrapper.Run(m_webFrame);

    if (!m_frame)
        return;

    m_format = convertVFCFormatToGLenum(*m_frame);

    if (m_format == GL_INVALID_VALUE) {
        m_provider->putCurrentFrame(m_webFrame);
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
        m_provider->putCurrentFrame(m_webFrame);
        m_frame = 0;
        return;
    }

    if (!copyPlaneData(resourceProvider)) {
        m_provider->putCurrentFrame(m_webFrame);
        m_frame = 0;
        return;
    }

    if (m_format == GL_TEXTURE_2D)
        m_externalTextureResource = resourceProvider->createResourceFromExternalTexture(m_frame->texture_id());
}

void VideoLayerImpl::appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData)
{
    DCHECK(Proxy::isImplThread());

    if (!m_frame)
        return;

    SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState, appendQuadsData);

    // FIXME: When we pass quads out of process, we need to double-buffer, or
    // otherwise synchonize use of all textures in the quad.

    gfx::Rect quadRect(contentBounds());

    switch (m_format) {
    case GL_LUMINANCE: {
        // YUV software decoder.
        const FramePlane& yPlane = m_framePlanes[media::VideoFrame::kYPlane];
        const FramePlane& uPlane = m_framePlanes[media::VideoFrame::kUPlane];
        const FramePlane& vPlane = m_framePlanes[media::VideoFrame::kVPlane];
        scoped_ptr<YUVVideoDrawQuad> yuvVideoQuad = YUVVideoDrawQuad::create(sharedQuadState, quadRect, yPlane, uPlane, vPlane);
        quadSink.append(yuvVideoQuad.PassAs<DrawQuad>(), appendQuadsData);
        break;
    }
    case GL_RGBA: {
        // RGBA software decoder.
        const FramePlane& plane = m_framePlanes[media::VideoFrame::kRGBPlane];
        bool premultipliedAlpha = true;
        float widthScaleFactor = static_cast<float>(plane.visibleSize.width()) / plane.size.width();
        gfx::RectF uvRect(widthScaleFactor, 1);
        bool flipped = false;
        scoped_ptr<TextureDrawQuad> textureQuad = TextureDrawQuad::create(sharedQuadState, quadRect, plane.resourceId, premultipliedAlpha, uvRect, flipped);
        quadSink.append(textureQuad.PassAs<DrawQuad>(), appendQuadsData);
        break;
    }
    case GL_TEXTURE_2D: {
        // NativeTexture hardware decoder.
        bool premultipliedAlpha = true;
        gfx::RectF uvRect(1, 1);
        bool flipped = false;
        scoped_ptr<TextureDrawQuad> textureQuad = TextureDrawQuad::create(sharedQuadState, quadRect, m_externalTextureResource, premultipliedAlpha, uvRect, flipped);
        quadSink.append(textureQuad.PassAs<DrawQuad>(), appendQuadsData);
        break;
    }
    case GL_TEXTURE_RECTANGLE_ARB: {
        scoped_ptr<IOSurfaceDrawQuad> ioSurfaceQuad = IOSurfaceDrawQuad::create(sharedQuadState, quadRect, m_frame->data_size(), m_frame->texture_id(), IOSurfaceDrawQuad::Unflipped);
        quadSink.append(ioSurfaceQuad.PassAs<DrawQuad>(), appendQuadsData);
        break;
    }
    case GL_TEXTURE_EXTERNAL_OES: {
        // StreamTexture hardware decoder.
        scoped_ptr<StreamVideoDrawQuad> streamVideoQuad = StreamVideoDrawQuad::create(sharedQuadState, quadRect, m_frame->texture_id(), m_streamTextureMatrix);
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
    DCHECK(Proxy::isImplThread());
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

    m_provider->putCurrentFrame(m_webFrame);
    m_frame = 0;

    m_providerLock.Release();
}

static int videoFrameDimension(int originalDimension, size_t plane, int format)
{
    if (format == media::VideoFrame::YV12 && plane != media::VideoFrame::kYPlane)
        return originalDimension / 2;
    return originalDimension;
}

static bool hasPaddingBytes(const media::VideoFrame& frame, size_t plane)
{
    return frame.stride(plane) > videoFrameDimension(frame.data_size().width(), plane, frame.format());
}

IntSize computeVisibleSize(const media::VideoFrame& frame, size_t plane)
{
    int visibleWidth = videoFrameDimension(frame.data_size().width(), plane, frame.format());
    int originalWidth = visibleWidth;
    int visibleHeight = videoFrameDimension(frame.data_size().height(), plane, frame.format());

    // When there are dead pixels at the edge of the texture, decrease
    // the frame width by 1 to prevent the rightmost pixels from
    // interpolating with the dead pixels.
    if (hasPaddingBytes(frame, plane))
        --visibleWidth;

    // In YV12, every 2x2 square of Y values corresponds to one U and
    // one V value. If we decrease the width of the UV plane, we must decrease the
    // width of the Y texture by 2 for proper alignment. This must happen
    // always, even if Y's texture does not have padding bytes.
    if (plane == media::VideoFrame::kYPlane && frame.format() == media::VideoFrame::YV12) {
        if (hasPaddingBytes(frame, media::VideoFrame::kUPlane))
            visibleWidth = originalWidth - 2;
    }

    return IntSize(visibleWidth, visibleHeight);
}

bool VideoLayerImpl::FramePlane::allocateData(ResourceProvider* resourceProvider)
{
    if (resourceId)
        return true;

    resourceId = resourceProvider->createResource(Renderer::ImplPool, size, format, ResourceProvider::TextureUsageAny);
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
    for (size_t planeIndex = 0; planeIndex < planeCount; ++planeIndex) {
        VideoLayerImpl::FramePlane& plane = m_framePlanes[planeIndex];

        IntSize requiredTextureSize(m_frame->stride(planeIndex), videoFrameDimension(m_frame->data_size().height(), planeIndex, m_frame->format()));
        // FIXME: Remove the test against maxTextureSize when tiled layers are implemented.
        if (requiredTextureSize.isZero() || requiredTextureSize.width() > maxTextureSize || requiredTextureSize.height() > maxTextureSize)
            return false;

        if (plane.size != requiredTextureSize || plane.format != m_format) {
            plane.freeData(resourceProvider);
            plane.size = requiredTextureSize;
            plane.format = m_format;
        }

        if (!plane.resourceId) {
            if (!plane.allocateData(resourceProvider))
                return false;
            plane.visibleSize = computeVisibleSize(*m_frame, planeIndex);
        }
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
        m_videoRenderer->Paint(m_frame, lock.skCanvas(), gfx::Rect(plane.size), 0xFF);
        return true;
    }

    for (size_t planeIndex = 0; planeIndex < planeCount; ++planeIndex) {
        VideoLayerImpl::FramePlane& plane = m_framePlanes[planeIndex];
        const uint8_t* softwarePlanePixels = m_frame->data(planeIndex);
        IntRect planeRect(IntPoint(), plane.size);
        resourceProvider->upload(plane.resourceId, softwarePlanePixels, planeRect, planeRect, IntSize());
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

void VideoLayerImpl::didReceiveFrame()
{
    setNeedsRedraw();
}

void VideoLayerImpl::didUpdateMatrix(const float matrix[16])
{
    m_streamTextureMatrix = WebKit::WebTransformationMatrix(
        matrix[0], matrix[1], matrix[2], matrix[3],
        matrix[4], matrix[5], matrix[6], matrix[7],
        matrix[8], matrix[9], matrix[10], matrix[11],
        matrix[12], matrix[13], matrix[14], matrix[15]);
    setNeedsRedraw();
}

void VideoLayerImpl::didLoseContext()
{
    freePlaneData(layerTreeHostImpl()->resourceProvider());
}

void VideoLayerImpl::setNeedsRedraw()
{
    layerTreeHostImpl()->setNeedsRedraw();
}

void VideoLayerImpl::dumpLayerProperties(std::string* str, int indent) const
{
    str->append(indentString(indent));
    str->append("video layer\n");
    LayerImpl::dumpLayerProperties(str, indent);
}

const char* VideoLayerImpl::layerTypeAsString() const
{
    return "VideoLayer";
}

}
