// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_PLUGIN_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_PLUGIN_H_

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
  static TestPlugin* create(blink::WebFrame* frame,
                            const blink::WebPluginParams& params,
                            WebTestDelegate* delegate);
  virtual ~TestPlugin();

  static const blink::WebString& MimeType();
  static const blink::WebString& CanCreateWithoutRendererMimeType();
  static const blink::WebString& PluginPersistsMimeType();
  static bool IsSupportedMimeType(const blink::WebString& mime_type);

  // WebPlugin methods:
  virtual bool initialize(blink::WebPluginContainer* container);
  virtual void destroy();
  virtual NPObject* scriptableObject();
  virtual bool canProcessDrag() const;
  virtual void paint(blink::WebCanvas* canvas, const blink::WebRect& rect) {}
  virtual void updateGeometry(
      const blink::WebRect& frame_rect,
      const blink::WebRect& clip_rect,
      const blink::WebVector<blink::WebRect>& cut_outs_rects,
      bool is_visible);
  virtual void updateFocus(bool focus) {}
  virtual void updateVisibility(bool visibility) {}
  virtual bool acceptsInputEvents();
  virtual bool handleInputEvent(const blink::WebInputEvent& event,
                                blink::WebCursorInfo& info);
  virtual bool handleDragStatusUpdate(blink::WebDragStatus drag_status,
                                      const blink::WebDragData& data,
                                      blink::WebDragOperationsMask mask,
                                      const blink::WebPoint& position,
                                      const blink::WebPoint& screen_position);
  virtual void didReceiveResponse(const blink::WebURLResponse& response) {}
  virtual void didReceiveData(const char* data, int data_length) {}
  virtual void didFinishLoading() {}
  virtual void didFailLoading(const blink::WebURLError& error) {}
  virtual void didFinishLoadingFrameRequest(const blink::WebURL& url,
                                            void* notify_data) {}
  virtual void didFailLoadingFrameRequest(const blink::WebURL& url,
                                          void* notify_data,
                                          const blink::WebURLError& error) {}
  virtual bool isPlaceholder();

  // cc::TextureLayerClient methods:
  virtual bool PrepareTextureMailbox(
      cc::TextureMailbox* mailbox,
      scoped_ptr<cc::SingleReleaseCallback>* release_callback,
      bool use_shared_memory) OVERRIDE;

 private:
  TestPlugin(blink::WebFrame* frame,
             const blink::WebPluginParams& params,
             WebTestDelegate* delegate);

  enum Primitive { PrimitiveNone, PrimitiveTriangle };

  struct Scene {
    Primitive primitive;
    unsigned background_color[3];
    unsigned primitive_color[3];
    float opacity;

    unsigned vbo;
    unsigned program;
    int color_location;
    int position_location;

    Scene()
        : primitive(PrimitiveNone),
          opacity(1.0f)  // Fully opaque.
          ,
          vbo(0),
          program(0),
          color_location(-1),
          position_location(-1) {
      background_color[0] = background_color[1] = background_color[2] = 0;
      primitive_color[0] = primitive_color[1] = primitive_color[2] = 0;
    }
  };

  // Functions for parsing plugin parameters.
  Primitive ParsePrimitive(const blink::WebString& string);
  void ParseColor(const blink::WebString& string, unsigned color[3]);
  float ParseOpacity(const blink::WebString& string);
  bool ParseBoolean(const blink::WebString& string);

  // Functions for loading and drawing scene in GL.
  bool InitScene();
  void DrawSceneGL();
  void DestroyScene();
  bool InitProgram();
  bool InitPrimitive();
  void DrawPrimitive();
  unsigned LoadShader(unsigned type, const std::string& source);
  unsigned LoadProgram(const std::string& vertex_source,
                       const std::string& fragment_source);

  // Functions for drawing scene in Software.
  void DrawSceneSoftware(void* memory, size_t bytes);

  blink::WebFrame* frame_;
  WebTestDelegate* delegate_;
  blink::WebPluginContainer* container_;

  blink::WebRect rect_;
  blink::WebGraphicsContext3D* context_;
  unsigned color_texture_;
  cc::TextureMailbox texture_mailbox_;
  scoped_ptr<base::SharedMemory> shared_bitmap_;
  bool mailbox_changed_;
  unsigned framebuffer_;
  Scene scene_;
  scoped_refptr<cc::TextureLayer> layer_;
  scoped_ptr<blink::WebLayer> web_layer_;

  blink::WebPluginContainer::TouchEventRequestType touch_event_request_;
  // Requests touch events from the WebPluginContainerImpl multiple times to
  // tickle webkit.org/b/108381
  bool re_request_touch_events_;
  bool print_event_details_;
  bool print_user_gesture_status_;
  bool can_process_drag_;

  bool is_persistent_;
  bool can_create_without_renderer_;

  DISALLOW_COPY_AND_ASSIGN(TestPlugin);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_PLUGIN_H_
