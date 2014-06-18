// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_TESTPLUGIN_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_TESTPLUGIN_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "content/public/test/layouttest_support.h"
#include "third_party/WebKit/public/platform/WebExternalTextureLayer.h"
#include "third_party/WebKit/public/platform/WebExternalTextureLayerClient.h"
#include "third_party/WebKit/public/platform/WebExternalTextureMailbox.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"

namespace blink {
class WebFrame;
class WebLayer;
}

namespace content {

class WebTestDelegate;

// A fake implemention of blink::WebPlugin for testing purposes.
//
// It uses WebGraphicsContext3D to paint a scene consisiting of a primitive
// over a background. The primitive and background can be customized using
// the following plugin parameters:
// primitive: none (default), triangle.
// background-color: black (default), red, green, blue.
// primitive-color: black (default), red, green, blue.
// opacity: [0.0 - 1.0]. Default is 1.0.
//
// Whether the plugin accepts touch events or not can be customized using the
// 'accepts-touch' plugin parameter (defaults to false).
class TestPlugin : public blink::WebPlugin, public cc::TextureLayerClient {
public:
    static TestPlugin* create(blink::WebFrame*, const blink::WebPluginParams&, WebTestDelegate*);
    virtual ~TestPlugin();

    static const blink::WebString& mimeType();
    static const blink::WebString& canCreateWithoutRendererMimeType();
    static const blink::WebString& pluginPersistsMimeType();
    static bool isSupportedMimeType(const blink::WebString& mimeType);

    // WebPlugin methods:
    virtual bool initialize(blink::WebPluginContainer*);
    virtual void destroy();
    virtual NPObject* scriptableObject();
    virtual bool canProcessDrag() const;
    virtual void paint(blink::WebCanvas*, const blink::WebRect&) { }
    virtual void updateGeometry(const blink::WebRect& frameRect, const blink::WebRect& clipRect, const blink::WebVector<blink::WebRect>& cutOutsRects, bool isVisible);
    virtual void updateFocus(bool) { }
    virtual void updateVisibility(bool) { }
    virtual bool acceptsInputEvents();
    virtual bool handleInputEvent(const blink::WebInputEvent&, blink::WebCursorInfo&);
    virtual bool handleDragStatusUpdate(blink::WebDragStatus, const blink::WebDragData&, blink::WebDragOperationsMask, const blink::WebPoint& position, const blink::WebPoint& screenPosition);
    virtual void didReceiveResponse(const blink::WebURLResponse&) { }
    virtual void didReceiveData(const char* data, int dataLength) { }
    virtual void didFinishLoading() { }
    virtual void didFailLoading(const blink::WebURLError&) { }
    virtual void didFinishLoadingFrameRequest(const blink::WebURL&, void* notifyData) { }
    virtual void didFailLoadingFrameRequest(const blink::WebURL&, void* notifyData, const blink::WebURLError&) { }
    virtual bool isPlaceholder();

    // cc::TextureLayerClient methods:
    virtual bool PrepareTextureMailbox(
        cc::TextureMailbox* mailbox,
        scoped_ptr<cc::SingleReleaseCallback>* releaseCallback,
        bool useSharedMemory) OVERRIDE;

private:
    TestPlugin(blink::WebFrame*, const blink::WebPluginParams&, WebTestDelegate*);

    enum Primitive {
        PrimitiveNone,
        PrimitiveTriangle
    };

    struct Scene {
        Primitive primitive;
        unsigned backgroundColor[3];
        unsigned primitiveColor[3];
        float opacity;

        unsigned vbo;
        unsigned program;
        int colorLocation;
        int positionLocation;

        Scene()
            : primitive(PrimitiveNone)
            , opacity(1.0f) // Fully opaque.
            , vbo(0)
            , program(0)
            , colorLocation(-1)
            , positionLocation(-1)
        {
            backgroundColor[0] = backgroundColor[1] = backgroundColor[2] = 0;
            primitiveColor[0] = primitiveColor[1] = primitiveColor[2] = 0;
        }
    };

    // Functions for parsing plugin parameters.
    Primitive parsePrimitive(const blink::WebString&);
    void parseColor(const blink::WebString&, unsigned color[3]);
    float parseOpacity(const blink::WebString&);
    bool parseBoolean(const blink::WebString&);

    // Functions for loading and drawing scene in GL.
    bool initScene();
    void drawSceneGL();
    void destroyScene();
    bool initProgram();
    bool initPrimitive();
    void drawPrimitive();
    unsigned loadShader(unsigned type, const std::string& source);
    unsigned loadProgram(const std::string& vertexSource, const std::string& fragmentSource);

    // Functions for drawing scene in Software.
    void drawSceneSoftware(void* memory, size_t bytes);

    blink::WebFrame* m_frame;
    WebTestDelegate* m_delegate;
    blink::WebPluginContainer* m_container;

    blink::WebRect m_rect;
    blink::WebGraphicsContext3D* m_context;
    unsigned m_colorTexture;
    cc::TextureMailbox m_textureMailbox;
    scoped_ptr<base::SharedMemory> m_sharedBitmap;
    bool m_mailboxChanged;
    unsigned m_framebuffer;
    Scene m_scene;
    scoped_refptr<cc::TextureLayer> m_layer;
    scoped_ptr<blink::WebLayer> m_webLayer;

    blink::WebPluginContainer::TouchEventRequestType m_touchEventRequest;
    // Requests touch events from the WebPluginContainerImpl multiple times to tickle webkit.org/b/108381
    bool m_reRequestTouchEvents;
    bool m_printEventDetails;
    bool m_printUserGestureStatus;
    bool m_canProcessDrag;

    bool m_isPersistent;
    bool m_canCreateWithoutRenderer;

    DISALLOW_COPY_AND_ASSIGN(TestPlugin);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_TESTPLUGIN_H_
