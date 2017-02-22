// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_TEST_PLUGIN_H_
#define CONTENT_SHELL_TEST_RUNNER_TEST_PLUGIN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {
class WebFrame;
class WebGraphicsContext3DProvider;
class WebLayer;
struct WebPluginParams;
}

namespace cc {
class SharedBitmap;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace test_runner {

class WebTestDelegate;

// A fake implemention of blink::WebPlugin for testing purposes.
//
// It uses GL to paint a scene consisiting of a primitive over a background. The
// primitive and background can be customized using the following plugin
// parameters.
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
  ~TestPlugin() override;

  static const blink::WebString& MimeType();
  static const blink::WebString& CanCreateWithoutRendererMimeType();
  static const blink::WebString& PluginPersistsMimeType();
  static bool IsSupportedMimeType(const blink::WebString& mime_type);

  // WebPlugin methods:
  bool initialize(blink::WebPluginContainer* container) override;
  void destroy() override;
  blink::WebPluginContainer* container() const override;
  bool canProcessDrag() const override;
  bool supportsKeyboardFocus() const override;
  void updateAllLifecyclePhases() override {}
  void paint(blink::WebCanvas* canvas, const blink::WebRect& rect) override {}
  void updateGeometry(const blink::WebRect& window_rect,
                      const blink::WebRect& clip_rect,
                      const blink::WebRect& unobscured_rect,
                      const blink::WebVector<blink::WebRect>& cut_outs_rects,
                      bool is_visible) override;
  void updateFocus(bool focus, blink::WebFocusType focus_type) override {}
  void updateVisibility(bool visibility) override {}
  blink::WebInputEventResult handleInputEvent(
      const blink::WebInputEvent& event,
      blink::WebCursorInfo& info) override;
  bool handleDragStatusUpdate(blink::WebDragStatus drag_status,
                              const blink::WebDragData& data,
                              blink::WebDragOperationsMask mask,
                              const blink::WebPoint& position,
                              const blink::WebPoint& screen_position) override;
  void didReceiveResponse(const blink::WebURLResponse& response) override {}
  void didReceiveData(const char* data, int data_length) override {}
  void didFinishLoading() override {}
  void didFailLoading(const blink::WebURLError& error) override {}
  bool isPlaceholder() override;

  // cc::TextureLayerClient methods:
  bool PrepareTextureMailbox(
      cc::TextureMailbox* mailbox,
      std::unique_ptr<cc::SingleReleaseCallback>* release_callback) override;

 private:
  TestPlugin(blink::WebFrame* frame,
             const blink::WebPluginParams& params,
             WebTestDelegate* delegate);

  enum Primitive { PrimitiveNone, PrimitiveTriangle };

  struct Scene {
    Primitive primitive;
    uint8_t background_color[3];
    uint8_t primitive_color[3];
    float opacity;

    GLuint vbo;
    GLuint program;
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
  void ParseColor(const blink::WebString& string, uint8_t color[3]);
  float ParseOpacity(const blink::WebString& string);
  bool ParseBoolean(const blink::WebString& string);

  // Functions for loading and drawing scene in GL.
  bool InitScene();
  void DrawSceneGL();
  void DestroyScene();
  bool InitProgram();
  bool InitPrimitive();
  void DrawPrimitive();
  GLuint LoadShader(GLenum type, const std::string& source);
  GLuint LoadProgram(const std::string& vertex_source,
                     const std::string& fragment_source);

  // Functions for drawing scene in Software.
  void DrawSceneSoftware(void* memory);

  blink::WebFrame* frame_;
  WebTestDelegate* delegate_;
  blink::WebPluginContainer* container_;

  blink::WebRect rect_;
  std::unique_ptr<blink::WebGraphicsContext3DProvider> context_provider_;
  gpu::gles2::GLES2Interface* gl_;
  GLuint color_texture_;
  cc::TextureMailbox texture_mailbox_;
  std::unique_ptr<cc::SharedBitmap> shared_bitmap_;
  bool mailbox_changed_;
  GLuint framebuffer_;
  Scene scene_;
  scoped_refptr<cc::TextureLayer> layer_;
  std::unique_ptr<blink::WebLayer> web_layer_;

  blink::WebPluginContainer::TouchEventRequestType touch_event_request_;
  // Requests touch events from the WebPluginContainerImpl multiple times to
  // tickle webkit.org/b/108381
  bool re_request_touch_events_;
  bool print_event_details_;
  bool print_user_gesture_status_;
  bool can_process_drag_;
  bool supports_keyboard_focus_;

  bool is_persistent_;
  bool can_create_without_renderer_;

  DISALLOW_COPY_AND_ASSIGN(TestPlugin);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_TEST_PLUGIN_H_
