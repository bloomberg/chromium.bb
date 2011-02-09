// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_scrollbar_widget.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/renderer/pepper_devices.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_device.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScrollbar.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "webkit/plugins/npapi/plugin_instance.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebRect;
using WebKit::WebScrollbar;
using WebKit::WebVector;


// Anonymous namespace for functions converting NPAPI to WebInputEvents types.
namespace {

WebKeyboardEvent BuildKeyEvent(const NPPepperEvent& event) {
  WebKeyboardEvent key_event;
  switch (event.type) {
    case NPEventType_RawKeyDown:
      key_event.type = WebInputEvent::RawKeyDown;
      break;
    case NPEventType_KeyDown:
      key_event.type = WebInputEvent::KeyDown;
      break;
    case NPEventType_KeyUp:
      key_event.type = WebInputEvent::KeyUp;
      break;
  }
  key_event.timeStampSeconds = event.timeStampSeconds;
  key_event.modifiers = event.u.key.modifier;
  key_event.windowsKeyCode = event.u.key.normalizedKeyCode;
  return key_event;
}

WebKeyboardEvent BuildCharEvent(const NPPepperEvent& event) {
  WebKeyboardEvent key_event;
  key_event.type = WebInputEvent::Char;
  key_event.timeStampSeconds = event.timeStampSeconds;
  key_event.modifiers = event.u.character.modifier;
  // For consistency, check that the sizes of the texts agree.
  DCHECK(sizeof(event.u.character.text) == sizeof(key_event.text));
  DCHECK(sizeof(event.u.character.unmodifiedText) ==
         sizeof(key_event.unmodifiedText));
  for (size_t i = 0; i < WebKeyboardEvent::textLengthCap; ++i) {
    key_event.text[i] = event.u.character.text[i];
    key_event.unmodifiedText[i] = event.u.character.unmodifiedText[i];
  }
  return key_event;
}

WebMouseEvent BuildMouseEvent(const NPPepperEvent& event) {
  WebMouseEvent mouse_event;
  switch (event.type) {
    case NPEventType_MouseDown:
      mouse_event.type = WebInputEvent::MouseDown;
      break;
    case NPEventType_MouseUp:
      mouse_event.type = WebInputEvent::MouseUp;
      break;
    case NPEventType_MouseMove:
      mouse_event.type = WebInputEvent::MouseMove;
      break;
    case NPEventType_MouseEnter:
      mouse_event.type = WebInputEvent::MouseEnter;
      break;
    case NPEventType_MouseLeave:
      mouse_event.type = WebInputEvent::MouseLeave;
      break;
  }
  mouse_event.timeStampSeconds = event.timeStampSeconds;
  mouse_event.modifiers = event.u.mouse.modifier;
  mouse_event.button = static_cast<WebMouseEvent::Button>(event.u.mouse.button);
  mouse_event.x = event.u.mouse.x;
  mouse_event.y = event.u.mouse.y;
  mouse_event.clickCount = event.u.mouse.clickCount;
  return mouse_event;
}

WebMouseWheelEvent BuildMouseWheelEvent(const NPPepperEvent& event) {
  WebMouseWheelEvent mouse_wheel_event;
  mouse_wheel_event.type = WebInputEvent::MouseWheel;
  mouse_wheel_event.timeStampSeconds = event.timeStampSeconds;
  mouse_wheel_event.modifiers = event.u.wheel.modifier;
  mouse_wheel_event.deltaX = event.u.wheel.deltaX;
  mouse_wheel_event.deltaY = event.u.wheel.deltaY;
  mouse_wheel_event.wheelTicksX = event.u.wheel.wheelTicksX;
  mouse_wheel_event.wheelTicksY = event.u.wheel.wheelTicksY;
  mouse_wheel_event.scrollByPage = event.u.wheel.scrollByPage;
  return mouse_wheel_event;
}

}  // namespace

PepperScrollbarWidget::PepperScrollbarWidget(
    const NPScrollbarCreateParams& params) {
  scrollbar_.reset(WebScrollbar::create(
    static_cast<WebKit::WebScrollbarClient*>(this),
    params.vertical ? WebScrollbar::Vertical : WebScrollbar::Horizontal));
  AddRef();
}

PepperScrollbarWidget::~PepperScrollbarWidget() {
}

void PepperScrollbarWidget::Destroy() {
  Release();
}

void PepperScrollbarWidget::Paint(Graphics2DDeviceContext* context,
                                  const NPRect& dirty) {
  gfx::Rect rect(dirty.left, dirty.top, dirty.right - dirty.left,
                 dirty.bottom - dirty.top);
  scrollbar_->paint(webkit_glue::ToWebCanvas(context->canvas()), rect);
  dirty_rect_ = dirty_rect_.Subtract(rect);
}

bool PepperScrollbarWidget::HandleEvent(const NPPepperEvent& event) {
  bool rv = false;

  switch (event.type) {
    case NPEventType_Undefined:
      return false;
    case NPEventType_MouseDown:
    case NPEventType_MouseUp:
    case NPEventType_MouseMove:
    case NPEventType_MouseEnter:
    case NPEventType_MouseLeave:
      rv = scrollbar_->handleInputEvent(BuildMouseEvent(event));
      break;
    case NPEventType_MouseWheel:
      rv = scrollbar_->handleInputEvent(BuildMouseWheelEvent(event));
      break;
    case NPEventType_RawKeyDown:
    case NPEventType_KeyDown:
    case NPEventType_KeyUp:
      rv = scrollbar_->handleInputEvent(BuildKeyEvent(event));
      break;
    case NPEventType_Char:
      rv = scrollbar_->handleInputEvent(BuildCharEvent(event));
      break;
    case NPEventType_Minimize:
    case NPEventType_Focus:
    case NPEventType_Device:
      // NOTIMPLEMENTED();
      break;
  }

  return rv;
}

void PepperScrollbarWidget::GetProperty(
    NPWidgetProperty property, void* value) {
  switch (property) {
    case NPWidgetPropertyLocation: {
      NPRect* rv = static_cast<NPRect*>(value);
      rv->left = location_.x();
      rv->top = location_.y();
      rv->right = location_.right();
      rv->bottom = location_.bottom();
      break;
    }
    case NPWidgetPropertyDirtyRect: {
      NPRect* rv = reinterpret_cast<NPRect*>(value);
      rv->left = dirty_rect_.x();
      rv->top = dirty_rect_.y();
      rv->right = dirty_rect_.right();
      rv->bottom = dirty_rect_.bottom();
      break;
    }
    case NPWidgetPropertyScrollbarThickness: {
      int32* rv = static_cast<int32*>(value);
      *rv = WebScrollbar::defaultThickness();
      break;
    }
    case NPWidgetPropertyScrollbarValue: {
      int32* rv = static_cast<int32*>(value);
      *rv = scrollbar_->value();
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void PepperScrollbarWidget::SetProperty(
    NPWidgetProperty property, void* value) {
  switch (property) {
    case NPWidgetPropertyLocation: {
      NPRect* r = static_cast<NPRect*>(value);
      location_ = gfx::Rect(
          r->left, r->top, r->right - r->left, r->bottom - r->top);
      scrollbar_->setLocation(location_);
      break;
    }
    case NPWidgetPropertyScrollbarValue: {
      int32* position = static_cast<int*>(value);
      scrollbar_->setValue(*position);
      break;
    }
    case NPWidgetPropertyScrollbarDocumentSize: {
      int32* total_length = static_cast<int32*>(value);
      scrollbar_->setDocumentSize(*total_length);
      break;
    }
    case NPWidgetPropertyScrollbarTickMarks: {
      NPScrollbarTickMarks* tickmarks =
          static_cast<NPScrollbarTickMarks*>(value);
      tickmarks_.resize(tickmarks->count);
      for (uint32 i = 0; i < tickmarks->count; ++i) {
        WebRect rect(
            tickmarks->tickmarks[i].left,
            tickmarks->tickmarks[i].top,
            tickmarks->tickmarks[i].right - tickmarks->tickmarks[i].left,
            tickmarks->tickmarks[i].bottom - tickmarks->tickmarks[i].top);
        tickmarks_[i] = rect;
      }
      dirty_rect_ = location_;
      NotifyInvalidate();
      break;
    }
    case NPWidgetPropertyScrollbarScrollByLine:
    case NPWidgetPropertyScrollbarScrollByPage:
    case NPWidgetPropertyScrollbarScrollByDocument:
    case NPWidgetPropertyScrollbarScrollByPixels: {
      bool forward;
      float multiplier = 1.0;

      WebScrollbar::ScrollGranularity granularity;
      if (property == NPWidgetPropertyScrollbarScrollByLine) {
        forward = *static_cast<bool*>(value);
        granularity = WebScrollbar::ScrollByLine;
      } else if (property == NPWidgetPropertyScrollbarScrollByPage) {
        forward = *static_cast<bool*>(value);
        granularity = WebScrollbar::ScrollByPage;
      } else if (property == NPWidgetPropertyScrollbarScrollByDocument) {
        forward = *static_cast<bool*>(value);
        granularity = WebScrollbar::ScrollByDocument;
      } else {
        multiplier = static_cast<float>(*static_cast<int32*>(value));
        forward = multiplier >= 0;
        if (multiplier < 0)
          multiplier *= -1;
        granularity = WebScrollbar::ScrollByPixel;
      }
      scrollbar_->scroll(
          forward ? WebScrollbar::ScrollForward : WebScrollbar::ScrollBackward,
          granularity, multiplier);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void PepperScrollbarWidget::valueChanged(WebScrollbar*) {
  WidgetPropertyChanged(NPWidgetPropertyScrollbarValue);
}

void PepperScrollbarWidget::invalidateScrollbarRect(WebScrollbar*,
                                                    const WebRect& rect) {
  dirty_rect_ = dirty_rect_.Union(rect);
  // Can't call into the client to tell them about the invalidate right away,
  // since the Scrollbar code is still in the middle of updating its internal
  // state.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PepperScrollbarWidget::NotifyInvalidate));
}

void PepperScrollbarWidget::getTickmarks(WebKit::WebScrollbar*,
                                         WebVector<WebRect>* tickmarks) const {
  if (tickmarks_.empty()) {
    WebRect* rects = NULL;
    tickmarks->assign(rects, 0);
  } else {
    tickmarks->assign(&tickmarks_[0], tickmarks_.size());
  }
}

void PepperScrollbarWidget::NotifyInvalidate() {
  if (!dirty_rect_.IsEmpty())
    WidgetPropertyChanged(NPWidgetPropertyDirtyRect);
}
