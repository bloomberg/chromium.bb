// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/test_plugin.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/strings/stringprintf.h"
#include "content/public/renderer/render_thread.h"
#include "content/shell/renderer/test_runner/web_test_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebCompositorSupport.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebTouchPoint.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

namespace content {

namespace {

// GLenum values copied from gl2.h.
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_ONE 1
#define GL_TRIANGLES 0x0004
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0B90
#define GL_TEXTURE_2D 0x0DE1
#define GL_FLOAT 0x1406
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER 0x8D40

void PremultiplyAlpha(const unsigned color_in[3],
                      float alpha,
                      float color_out[4]) {
  for (int i = 0; i < 3; ++i)
    color_out[i] = (color_in[i] / 255.0f) * alpha;

  color_out[3] = alpha;
}

const char* PointState(blink::WebTouchPoint::State state) {
  switch (state) {
    case blink::WebTouchPoint::StateReleased:
      return "Released";
    case blink::WebTouchPoint::StatePressed:
      return "Pressed";
    case blink::WebTouchPoint::StateMoved:
      return "Moved";
    case blink::WebTouchPoint::StateCancelled:
      return "Cancelled";
    default:
      return "Unknown";
  }
}

void PrintTouchList(WebTestDelegate* delegate,
                    const blink::WebTouchPoint* points,
                    int length) {
  for (int i = 0; i < length; ++i) {
    delegate->PrintMessage(base::StringPrintf("* %.2f, %.2f: %s\n",
                                              points[i].position.x,
                                              points[i].position.y,
                                              PointState(points[i].state)));
  }
}

void PrintEventDetails(WebTestDelegate* delegate,
                       const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::isTouchEventType(event.type)) {
    const blink::WebTouchEvent& touch =
        static_cast<const blink::WebTouchEvent&>(event);
    PrintTouchList(delegate, touch.touches, touch.touchesLength);
  } else if (blink::WebInputEvent::isMouseEventType(event.type) ||
             event.type == blink::WebInputEvent::MouseWheel) {
    const blink::WebMouseEvent& mouse =
        static_cast<const blink::WebMouseEvent&>(event);
    delegate->PrintMessage(base::StringPrintf("* %d, %d\n", mouse.x, mouse.y));
  } else if (blink::WebInputEvent::isGestureEventType(event.type)) {
    const blink::WebGestureEvent& gesture =
        static_cast<const blink::WebGestureEvent&>(event);
    delegate->PrintMessage(
        base::StringPrintf("* %d, %d\n", gesture.x, gesture.y));
  }
}

blink::WebPluginContainer::TouchEventRequestType ParseTouchEventRequestType(
    const blink::WebString& string) {
  if (string == blink::WebString::fromUTF8("raw"))
    return blink::WebPluginContainer::TouchEventRequestTypeRaw;
  if (string == blink::WebString::fromUTF8("synthetic"))
    return blink::WebPluginContainer::TouchEventRequestTypeSynthesizedMouse;
  return blink::WebPluginContainer::TouchEventRequestTypeNone;
}

void DeferredDelete(void* context) {
  TestPlugin* plugin = static_cast<TestPlugin*>(context);
  delete plugin;
}

}  // namespace

TestPlugin::TestPlugin(blink::WebFrame* frame,
                       const blink::WebPluginParams& params,
                       WebTestDelegate* delegate)
    : frame_(frame),
      delegate_(delegate),
      container_(0),
      context_(0),
      color_texture_(0),
      mailbox_changed_(false),
      framebuffer_(0),
      touch_event_request_(
          blink::WebPluginContainer::TouchEventRequestTypeNone),
      re_request_touch_events_(false),
      print_event_details_(false),
      print_user_gesture_status_(false),
      can_process_drag_(false),
      is_persistent_(params.mimeType == PluginPersistsMimeType()),
      can_create_without_renderer_(params.mimeType ==
                                   CanCreateWithoutRendererMimeType()) {
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString, kAttributePrimitive, ("primitive"));
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString, kAttributeBackgroundColor, ("background-color"));
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString, kAttributePrimitiveColor, ("primitive-color"));
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString, kAttributeOpacity, ("opacity"));
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString, kAttributeAcceptsTouch, ("accepts-touch"));
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString, kAttributeReRequestTouchEvents, ("re-request-touch"));
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString, kAttributePrintEventDetails, ("print-event-details"));
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString, kAttributeCanProcessDrag, ("can-process-drag"));
  const CR_DEFINE_STATIC_LOCAL(blink::WebString,
                               kAttributePrintUserGestureStatus,
                               ("print-user-gesture-status"));

  DCHECK_EQ(params.attributeNames.size(), params.attributeValues.size());
  size_t size = params.attributeNames.size();
  for (size_t i = 0; i < size; ++i) {
    const blink::WebString& attribute_name = params.attributeNames[i];
    const blink::WebString& attribute_value = params.attributeValues[i];

    if (attribute_name == kAttributePrimitive)
      scene_.primitive = ParsePrimitive(attribute_value);
    else if (attribute_name == kAttributeBackgroundColor)
      ParseColor(attribute_value, scene_.background_color);
    else if (attribute_name == kAttributePrimitiveColor)
      ParseColor(attribute_value, scene_.primitive_color);
    else if (attribute_name == kAttributeOpacity)
      scene_.opacity = ParseOpacity(attribute_value);
    else if (attribute_name == kAttributeAcceptsTouch)
      touch_event_request_ = ParseTouchEventRequestType(attribute_value);
    else if (attribute_name == kAttributeReRequestTouchEvents)
      re_request_touch_events_ = ParseBoolean(attribute_value);
    else if (attribute_name == kAttributePrintEventDetails)
      print_event_details_ = ParseBoolean(attribute_value);
    else if (attribute_name == kAttributeCanProcessDrag)
      can_process_drag_ = ParseBoolean(attribute_value);
    else if (attribute_name == kAttributePrintUserGestureStatus)
      print_user_gesture_status_ = ParseBoolean(attribute_value);
  }
  if (can_create_without_renderer_)
    delegate_->PrintMessage(
        std::string("TestPlugin: canCreateWithoutRenderer\n"));
}

TestPlugin::~TestPlugin() {
}

bool TestPlugin::initialize(blink::WebPluginContainer* container) {
  blink::WebGraphicsContext3D::Attributes attrs;
  context_ =
      blink::Platform::current()->createOffscreenGraphicsContext3D(attrs);

  if (!InitScene())
    return false;

  layer_ = cc::TextureLayer::CreateForMailbox(this);
  web_layer_ = make_scoped_ptr(InstantiateWebLayer(layer_));
  container_ = container;
  container_->setWebLayer(web_layer_.get());
  if (re_request_touch_events_) {
    container_->requestTouchEventType(
        blink::WebPluginContainer::TouchEventRequestTypeSynthesizedMouse);
    container_->requestTouchEventType(
        blink::WebPluginContainer::TouchEventRequestTypeRaw);
  }
  container_->requestTouchEventType(touch_event_request_);
  container_->setWantsWheelEvents(true);
  return true;
}

void TestPlugin::destroy() {
  if (layer_.get())
    layer_->ClearTexture();
  if (container_)
    container_->setWebLayer(0);
  web_layer_.reset();
  layer_ = NULL;
  DestroyScene();

  delete context_;
  context_ = 0;

  container_ = 0;
  frame_ = 0;

  blink::Platform::current()->callOnMainThread(DeferredDelete, this);
}

NPObject* TestPlugin::scriptableObject() {
  return 0;
}

bool TestPlugin::canProcessDrag() const {
  return can_process_drag_;
}

void TestPlugin::updateGeometry(
    const blink::WebRect& frame_rect,
    const blink::WebRect& clip_rect,
    const blink::WebVector<blink::WebRect>& cut_outs_rects,
    bool is_visible) {
  if (clip_rect == rect_)
    return;
  rect_ = clip_rect;

  if (rect_.isEmpty()) {
    texture_mailbox_ = cc::TextureMailbox();
  } else if (context_) {
    context_->viewport(0, 0, rect_.width, rect_.height);

    context_->bindTexture(GL_TEXTURE_2D, color_texture_);
    context_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    context_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    context_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    context_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    context_->texImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA,
                         rect_.width,
                         rect_.height,
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         0);
    context_->bindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    context_->framebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture_, 0);

    DrawSceneGL();

    gpu::Mailbox mailbox;
    context_->genMailboxCHROMIUM(mailbox.name);
    context_->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    context_->flush();
    uint32 sync_point = context_->insertSyncPoint();
    texture_mailbox_ = cc::TextureMailbox(mailbox, GL_TEXTURE_2D, sync_point);
  } else {
    size_t bytes = 4 * rect_.width * rect_.height;
    scoped_ptr<base::SharedMemory> bitmap =
        RenderThread::Get()->HostAllocateSharedMemoryBuffer(bytes);
    if (!bitmap->Map(bytes)) {
      texture_mailbox_ = cc::TextureMailbox();
    } else {
      DrawSceneSoftware(bitmap->memory(), bytes);
      texture_mailbox_ = cc::TextureMailbox(
          bitmap.get(), gfx::Size(rect_.width, rect_.height));
      shared_bitmap_ = bitmap.Pass();
    }
  }

  mailbox_changed_ = true;
  layer_->SetNeedsDisplay();
}

bool TestPlugin::acceptsInputEvents() {
  return true;
}

bool TestPlugin::isPlaceholder() {
  return false;
}

static void IgnoreReleaseCallback(uint32 sync_point, bool lost) {
}

static void ReleaseSharedMemory(scoped_ptr<base::SharedMemory> bitmap,
                                uint32 sync_point,
                                bool lost) {
}

bool TestPlugin::PrepareTextureMailbox(
    cc::TextureMailbox* mailbox,
    scoped_ptr<cc::SingleReleaseCallback>* release_callback,
    bool use_shared_memory) {
  if (!mailbox_changed_)
    return false;
  *mailbox = texture_mailbox_;
  if (texture_mailbox_.IsTexture()) {
    *release_callback =
        cc::SingleReleaseCallback::Create(base::Bind(&IgnoreReleaseCallback));
  } else {
    *release_callback = cc::SingleReleaseCallback::Create(
        base::Bind(&ReleaseSharedMemory, base::Passed(&shared_bitmap_)));
  }
  mailbox_changed_ = false;
  return true;
}

TestPlugin::Primitive TestPlugin::ParsePrimitive(
    const blink::WebString& string) {
  const CR_DEFINE_STATIC_LOCAL(blink::WebString, kPrimitiveNone, ("none"));
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString, kPrimitiveTriangle, ("triangle"));

  Primitive primitive = PrimitiveNone;
  if (string == kPrimitiveNone)
    primitive = PrimitiveNone;
  else if (string == kPrimitiveTriangle)
    primitive = PrimitiveTriangle;
  else
    NOTREACHED();
  return primitive;
}

// FIXME: This method should already exist. Use it.
// For now just parse primary colors.
void TestPlugin::ParseColor(const blink::WebString& string, unsigned color[3]) {
  color[0] = color[1] = color[2] = 0;
  if (string == "black")
    return;

  if (string == "red")
    color[0] = 255;
  else if (string == "green")
    color[1] = 255;
  else if (string == "blue")
    color[2] = 255;
  else
    NOTREACHED();
}

float TestPlugin::ParseOpacity(const blink::WebString& string) {
  return static_cast<float>(atof(string.utf8().data()));
}

bool TestPlugin::ParseBoolean(const blink::WebString& string) {
  const CR_DEFINE_STATIC_LOCAL(blink::WebString, kPrimitiveTrue, ("true"));
  return string == kPrimitiveTrue;
}

bool TestPlugin::InitScene() {
  if (!context_)
    return true;

  float color[4];
  PremultiplyAlpha(scene_.background_color, scene_.opacity, color);

  color_texture_ = context_->createTexture();
  framebuffer_ = context_->createFramebuffer();

  context_->viewport(0, 0, rect_.width, rect_.height);
  context_->disable(GL_DEPTH_TEST);
  context_->disable(GL_SCISSOR_TEST);

  context_->clearColor(color[0], color[1], color[2], color[3]);

  context_->enable(GL_BLEND);
  context_->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  return scene_.primitive != PrimitiveNone ? InitProgram() && InitPrimitive()
                                           : true;
}

void TestPlugin::DrawSceneGL() {
  context_->viewport(0, 0, rect_.width, rect_.height);
  context_->clear(GL_COLOR_BUFFER_BIT);

  if (scene_.primitive != PrimitiveNone)
    DrawPrimitive();
}

void TestPlugin::DrawSceneSoftware(void* memory, size_t bytes) {
  DCHECK_EQ(bytes, rect_.width * rect_.height * 4u);

  SkColor background_color =
      SkColorSetARGB(static_cast<uint8>(scene_.opacity * 255),
                     scene_.background_color[0],
                     scene_.background_color[1],
                     scene_.background_color[2]);

  const SkImageInfo info =
      SkImageInfo::MakeN32Premul(rect_.width, rect_.height);
  SkBitmap bitmap;
  bitmap.installPixels(info, memory, info.minRowBytes());
  SkCanvas canvas(bitmap);
  canvas.clear(background_color);

  if (scene_.primitive != PrimitiveNone) {
    DCHECK_EQ(PrimitiveTriangle, scene_.primitive);
    SkColor foreground_color =
        SkColorSetARGB(static_cast<uint8>(scene_.opacity * 255),
                       scene_.primitive_color[0],
                       scene_.primitive_color[1],
                       scene_.primitive_color[2]);
    SkPath triangle_path;
    triangle_path.moveTo(0.5f * rect_.width, 0.9f * rect_.height);
    triangle_path.lineTo(0.1f * rect_.width, 0.1f * rect_.height);
    triangle_path.lineTo(0.9f * rect_.width, 0.1f * rect_.height);
    SkPaint paint;
    paint.setColor(foreground_color);
    paint.setStyle(SkPaint::kFill_Style);
    canvas.drawPath(triangle_path, paint);
  }
}

void TestPlugin::DestroyScene() {
  if (scene_.program) {
    context_->deleteProgram(scene_.program);
    scene_.program = 0;
  }
  if (scene_.vbo) {
    context_->deleteBuffer(scene_.vbo);
    scene_.vbo = 0;
  }

  if (framebuffer_) {
    context_->deleteFramebuffer(framebuffer_);
    framebuffer_ = 0;
  }

  if (color_texture_) {
    context_->deleteTexture(color_texture_);
    color_texture_ = 0;
  }
}

bool TestPlugin::InitProgram() {
  const std::string vertex_source(
      "attribute vec4 position;  \n"
      "void main() {             \n"
      "  gl_Position = position; \n"
      "}                         \n");

  const std::string fragment_source(
      "precision mediump float; \n"
      "uniform vec4 color;      \n"
      "void main() {            \n"
      "  gl_FragColor = color;  \n"
      "}                        \n");

  scene_.program = LoadProgram(vertex_source, fragment_source);
  if (!scene_.program)
    return false;

  scene_.color_location = context_->getUniformLocation(scene_.program, "color");
  scene_.position_location =
      context_->getAttribLocation(scene_.program, "position");
  return true;
}

bool TestPlugin::InitPrimitive() {
  DCHECK_EQ(scene_.primitive, PrimitiveTriangle);

  scene_.vbo = context_->createBuffer();
  if (!scene_.vbo)
    return false;

  const float vertices[] = {0.0f, 0.8f, 0.0f,  -0.8f, -0.8f,
                            0.0f, 0.8f, -0.8f, 0.0f};
  context_->bindBuffer(GL_ARRAY_BUFFER, scene_.vbo);
  context_->bufferData(GL_ARRAY_BUFFER, sizeof(vertices), 0, GL_STATIC_DRAW);
  context_->bufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
  return true;
}

void TestPlugin::DrawPrimitive() {
  DCHECK_EQ(scene_.primitive, PrimitiveTriangle);
  DCHECK(scene_.vbo);
  DCHECK(scene_.program);

  context_->useProgram(scene_.program);

  // Bind primitive color.
  float color[4];
  PremultiplyAlpha(scene_.primitive_color, scene_.opacity, color);
  context_->uniform4f(
      scene_.color_location, color[0], color[1], color[2], color[3]);

  // Bind primitive vertices.
  context_->bindBuffer(GL_ARRAY_BUFFER, scene_.vbo);
  context_->enableVertexAttribArray(scene_.position_location);
  context_->vertexAttribPointer(
      scene_.position_location, 3, GL_FLOAT, GL_FALSE, 0, 0);
  context_->drawArrays(GL_TRIANGLES, 0, 3);
}

unsigned TestPlugin::LoadShader(unsigned type, const std::string& source) {
  unsigned shader = context_->createShader(type);
  if (shader) {
    context_->shaderSource(shader, source.data());
    context_->compileShader(shader);

    int compiled = 0;
    context_->getShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
      context_->deleteShader(shader);
      shader = 0;
    }
  }
  return shader;
}

unsigned TestPlugin::LoadProgram(const std::string& vertex_source,
                                 const std::string& fragment_source) {
  unsigned vertex_shader = LoadShader(GL_VERTEX_SHADER, vertex_source);
  unsigned fragment_shader = LoadShader(GL_FRAGMENT_SHADER, fragment_source);
  unsigned program = context_->createProgram();
  if (vertex_shader && fragment_shader && program) {
    context_->attachShader(program, vertex_shader);
    context_->attachShader(program, fragment_shader);
    context_->linkProgram(program);

    int linked = 0;
    context_->getProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
      context_->deleteProgram(program);
      program = 0;
    }
  }
  if (vertex_shader)
    context_->deleteShader(vertex_shader);
  if (fragment_shader)
    context_->deleteShader(fragment_shader);

  return program;
}

bool TestPlugin::handleInputEvent(const blink::WebInputEvent& event,
                                  blink::WebCursorInfo& info) {
  const char* event_name = 0;
  switch (event.type) {
    case blink::WebInputEvent::Undefined:
      event_name = "unknown";
      break;

    case blink::WebInputEvent::MouseDown:
      event_name = "MouseDown";
      break;
    case blink::WebInputEvent::MouseUp:
      event_name = "MouseUp";
      break;
    case blink::WebInputEvent::MouseMove:
      event_name = "MouseMove";
      break;
    case blink::WebInputEvent::MouseEnter:
      event_name = "MouseEnter";
      break;
    case blink::WebInputEvent::MouseLeave:
      event_name = "MouseLeave";
      break;
    case blink::WebInputEvent::ContextMenu:
      event_name = "ContextMenu";
      break;

    case blink::WebInputEvent::MouseWheel:
      event_name = "MouseWheel";
      break;

    case blink::WebInputEvent::RawKeyDown:
      event_name = "RawKeyDown";
      break;
    case blink::WebInputEvent::KeyDown:
      event_name = "KeyDown";
      break;
    case blink::WebInputEvent::KeyUp:
      event_name = "KeyUp";
      break;
    case blink::WebInputEvent::Char:
      event_name = "Char";
      break;

    case blink::WebInputEvent::GestureScrollBegin:
      event_name = "GestureScrollBegin";
      break;
    case blink::WebInputEvent::GestureScrollEnd:
      event_name = "GestureScrollEnd";
      break;
    case blink::WebInputEvent::GestureScrollUpdateWithoutPropagation:
    case blink::WebInputEvent::GestureScrollUpdate:
      event_name = "GestureScrollUpdate";
      break;
    case blink::WebInputEvent::GestureFlingStart:
      event_name = "GestureFlingStart";
      break;
    case blink::WebInputEvent::GestureFlingCancel:
      event_name = "GestureFlingCancel";
      break;
    case blink::WebInputEvent::GestureTap:
      event_name = "GestureTap";
      break;
    case blink::WebInputEvent::GestureTapUnconfirmed:
      event_name = "GestureTapUnconfirmed";
      break;
    case blink::WebInputEvent::GestureTapDown:
      event_name = "GestureTapDown";
      break;
    case blink::WebInputEvent::GestureShowPress:
      event_name = "GestureShowPress";
      break;
    case blink::WebInputEvent::GestureTapCancel:
      event_name = "GestureTapCancel";
      break;
    case blink::WebInputEvent::GestureDoubleTap:
      event_name = "GestureDoubleTap";
      break;
    case blink::WebInputEvent::GestureTwoFingerTap:
      event_name = "GestureTwoFingerTap";
      break;
    case blink::WebInputEvent::GestureLongPress:
      event_name = "GestureLongPress";
      break;
    case blink::WebInputEvent::GestureLongTap:
      event_name = "GestureLongTap";
      break;
    case blink::WebInputEvent::GesturePinchBegin:
      event_name = "GesturePinchBegin";
      break;
    case blink::WebInputEvent::GesturePinchEnd:
      event_name = "GesturePinchEnd";
      break;
    case blink::WebInputEvent::GesturePinchUpdate:
      event_name = "GesturePinchUpdate";
      break;

    case blink::WebInputEvent::TouchStart:
      event_name = "TouchStart";
      break;
    case blink::WebInputEvent::TouchMove:
      event_name = "TouchMove";
      break;
    case blink::WebInputEvent::TouchEnd:
      event_name = "TouchEnd";
      break;
    case blink::WebInputEvent::TouchCancel:
      event_name = "TouchCancel";
      break;
  }

  delegate_->PrintMessage(std::string("Plugin received event: ") +
                          (event_name ? event_name : "unknown") + "\n");
  if (print_event_details_)
    PrintEventDetails(delegate_, event);
  if (print_user_gesture_status_)
    delegate_->PrintMessage(
        std::string("* ") +
        (blink::WebUserGestureIndicator::isProcessingUserGesture() ? ""
                                                                   : "not ") +
        "handling user gesture\n");
  if (is_persistent_)
    delegate_->PrintMessage(std::string("TestPlugin: isPersistent\n"));
  return false;
}

bool TestPlugin::handleDragStatusUpdate(
    blink::WebDragStatus drag_status,
    const blink::WebDragData& data,
    blink::WebDragOperationsMask mask,
    const blink::WebPoint& position,
    const blink::WebPoint& screen_position) {
  const char* drag_status_name = 0;
  switch (drag_status) {
    case blink::WebDragStatusEnter:
      drag_status_name = "DragEnter";
      break;
    case blink::WebDragStatusOver:
      drag_status_name = "DragOver";
      break;
    case blink::WebDragStatusLeave:
      drag_status_name = "DragLeave";
      break;
    case blink::WebDragStatusDrop:
      drag_status_name = "DragDrop";
      break;
    case blink::WebDragStatusUnknown:
      NOTREACHED();
  }
  delegate_->PrintMessage(std::string("Plugin received event: ") +
                          drag_status_name + "\n");
  return false;
}

TestPlugin* TestPlugin::create(blink::WebFrame* frame,
                               const blink::WebPluginParams& params,
                               WebTestDelegate* delegate) {
  return new TestPlugin(frame, params, delegate);
}

const blink::WebString& TestPlugin::MimeType() {
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString, kMimeType, ("application/x-webkit-test-webplugin"));
  return kMimeType;
}

const blink::WebString& TestPlugin::CanCreateWithoutRendererMimeType() {
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString,
      kCanCreateWithoutRendererMimeType,
      ("application/x-webkit-test-webplugin-can-create-without-renderer"));
  return kCanCreateWithoutRendererMimeType;
}

const blink::WebString& TestPlugin::PluginPersistsMimeType() {
  const CR_DEFINE_STATIC_LOCAL(
      blink::WebString,
      kPluginPersistsMimeType,
      ("application/x-webkit-test-webplugin-persistent"));
  return kPluginPersistsMimeType;
}

bool TestPlugin::IsSupportedMimeType(const blink::WebString& mime_type) {
  return mime_type == TestPlugin::MimeType() ||
         mime_type == PluginPersistsMimeType() ||
         mime_type == CanCreateWithoutRendererMimeType();
}

}  // namespace content
