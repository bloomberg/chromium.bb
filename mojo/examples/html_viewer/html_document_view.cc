// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/html_viewer/html_document_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "mojo/examples/html_viewer/webstoragenamespace_impl.h"
#include "mojo/examples/html_viewer/weburlloader_impl.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/events/event_constants.h"

namespace mojo {
namespace examples {
namespace {

int EventFlagsToWebEventModifiers(int flags) {
  int modifiers = 0;

  if (flags & ui::EF_SHIFT_DOWN)
    modifiers |= blink::WebInputEvent::ShiftKey;
  if (flags & ui::EF_CONTROL_DOWN)
    modifiers |= blink::WebInputEvent::ControlKey;
  if (flags & ui::EF_ALT_DOWN)
    modifiers |= blink::WebInputEvent::AltKey;
  // TODO(beng): MetaKey/META_MASK
  if (flags & ui::EF_LEFT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::LeftButtonDown;
  if (flags & ui::EF_MIDDLE_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::MiddleButtonDown;
  if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::RightButtonDown;
  if (flags & ui::EF_CAPS_LOCK_DOWN)
    modifiers |= blink::WebInputEvent::CapsLockOn;
  return modifiers;
}

int GetClickCount(int flags) {
  if (flags & ui::EF_IS_TRIPLE_CLICK)
    return 3;
  else if (flags & ui::EF_IS_DOUBLE_CLICK)
    return 2;

  return 1;
}

void ConfigureSettings(blink::WebSettings* settings) {
  settings->setAcceleratedCompositingEnabled(false);
  settings->setDefaultFixedFontSize(13);
  settings->setDefaultFontSize(16);
  settings->setLoadsImagesAutomatically(true);
  settings->setJavaScriptEnabled(true);
}

}  // namespace

HTMLDocumentView::HTMLDocumentView(view_manager::ViewManager* view_manager)
    : view_manager_(view_manager),
      view_(view_manager::View::Create(view_manager_)),
      web_view_(NULL),
      repaint_pending_(false),
      weak_factory_(this) {
  view_->AddObserver(this);
}

HTMLDocumentView::~HTMLDocumentView() {
  view_->RemoveObserver(this);

  if (web_view_)
    web_view_->close();
}

void HTMLDocumentView::AttachToNode(view_manager::Node* node) {
  node->SetActiveView(view_);
  view_->SetColor(SK_ColorCYAN);  // Dummy background color.

  web_view_ = blink::WebView::create(this);
  ConfigureSettings(web_view_->settings());
  web_view_->setMainFrame(blink::WebLocalFrame::create(this));

  // TODO(darin): Track size of view_manager::Node.
  web_view_->resize(gfx::Size(800, 600));
}

void HTMLDocumentView::Load(URLResponsePtr response,
                            ScopedDataPipeConsumerHandle response_body_stream) {
  DCHECK(web_view_);

  GURL url(response->url);

  WebURLRequestExtraData* extra_data = new WebURLRequestExtraData;
  extra_data->synthetic_response = response.Pass();
  extra_data->synthetic_response_body_stream = response_body_stream.Pass();

  blink::WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(url);
  web_request.setExtraData(extra_data);

  web_view_->mainFrame()->loadRequest(web_request);
}

blink::WebStorageNamespace* HTMLDocumentView::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

void HTMLDocumentView::didInvalidateRect(const blink::WebRect& rect) {
  if (!repaint_pending_) {
    repaint_pending_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&HTMLDocumentView::Repaint, weak_factory_.GetWeakPtr()));
  }
}

bool HTMLDocumentView::allowsBrokenNullLayerTreeView() const {
  // TODO(darin): Switch to using compositor bindings.
  //
  // NOTE: Note to Blink maintainers, feel free to just break this code if it
  // is the last using compositor bindings and you want to delete the old path.
  //
  return true;
}

void HTMLDocumentView::didAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  printf("### console: %s\n", std::string(message.text.utf8()).c_str());
}

void HTMLDocumentView::OnViewInputEvent(view_manager::View* view,
                                        const EventPtr& event) {
  if (event->action != ui::ET_MOUSE_PRESSED &&
      event->action != ui::ET_MOUSE_RELEASED &&
      event->action != ui::ET_MOUSE_ENTERED &&
      event->action != ui::ET_MOUSE_EXITED &&
      event->action != ui::ET_MOUSE_MOVED &&
      event->action != ui::ET_MOUSE_DRAGGED) {
    return;
  }

  blink::WebMouseEvent web_event;
  web_event.x = event->location->x;
  web_event.y = event->location->y;

  web_event.modifiers = EventFlagsToWebEventModifiers(event->flags);
  web_event.timeStampSeconds =
      base::TimeDelta::FromInternalValue(event->time_stamp).InSecondsF();

  web_event.button = blink::WebMouseEvent::ButtonNone;
  if (event->flags & ui::EF_LEFT_MOUSE_BUTTON)
    web_event.button = blink::WebMouseEvent::ButtonLeft;
  if (event->flags & ui::EF_MIDDLE_MOUSE_BUTTON)
    web_event.button = blink::WebMouseEvent::ButtonMiddle;
  if (event->flags & ui::EF_RIGHT_MOUSE_BUTTON)
    web_event.button = blink::WebMouseEvent::ButtonRight;

  switch (event->action) {
    case ui::ET_MOUSE_PRESSED:
      web_event.type = blink::WebInputEvent::MouseDown;
      web_event.clickCount = GetClickCount(event->flags);
      break;
    case ui::ET_MOUSE_RELEASED:
      web_event.type = blink::WebInputEvent::MouseUp;
      web_event.clickCount = GetClickCount(event->flags);
      break;
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      web_event.type = blink::WebInputEvent::MouseMove;
      break;
    default:
      NOTIMPLEMENTED() << "Received unexpected event: " << event->action;
      break;
  }

  web_view_->handleInputEvent(web_event);
}

void HTMLDocumentView::Repaint() {
  repaint_pending_ = false;

  web_view_->animate(0.0);
  web_view_->layout();

  int width = web_view_->size().width;
  int height = web_view_->size().height;

  skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(SkCanvas::NewRaster(
      SkImageInfo::MakeN32(width, height, kOpaque_SkAlphaType)));

  web_view_->paint(canvas.get(), gfx::Rect(0, 0, width, height));

  view_->SetContents(canvas->getDevice()->accessBitmap(false));
}

}  // namespace examples
}  // namespace mojo
