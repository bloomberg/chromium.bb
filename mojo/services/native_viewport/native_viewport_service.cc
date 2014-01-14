// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_service.h"

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "mojo/services/gles2/gles2_impl.h"
#include "mojo/services/native_viewport/native_viewport.h"
#include "mojom/native_viewport.h"
#include "ui/events/event.h"

namespace mojo {
namespace services {
namespace {

bool IsRateLimitedEventType(ui::Event* event) {
  return event->type() == ui::ET_MOUSE_MOVED ||
         event->type() == ui::ET_MOUSE_DRAGGED ||
         event->type() == ui::ET_TOUCH_MOVED;
}

}

class NativeViewportService::NativeViewportImpl
    : public mojo::NativeViewport,
      public NativeViewportDelegate {
 public:
  NativeViewportImpl(NativeViewportService* service,
                     ScopedMessagePipeHandle client_handle)
      : service_(service),
        widget_(gfx::kNullAcceleratedWidget),
        waiting_for_event_ack_(false),
        pending_event_timestamp_(0),
        client_(client_handle.Pass(), this) {
  }
  virtual ~NativeViewportImpl() {}

  virtual void Open() MOJO_OVERRIDE {
    native_viewport_ =
        services::NativeViewport::Create(service_->context_, this);
    native_viewport_->Init();
    client_->OnCreated();
  }

  virtual void Close() MOJO_OVERRIDE {
    gles2_.reset();
    DCHECK(native_viewport_);
    native_viewport_->Close();
  }

  virtual void CreateGLES2Context(ScopedMessagePipeHandle client_handle)
      MOJO_OVERRIDE {
    gles2_.reset(new GLES2Impl(client_handle.Pass()));
    CreateGLES2ContextIfNeeded();
  }

  virtual void AckEvent(const Event& event) MOJO_OVERRIDE {
    DCHECK_EQ(event.time_stamp(), pending_event_timestamp_);
    waiting_for_event_ack_ = false;
  }

  void CreateGLES2ContextIfNeeded() {
    if (widget_ == gfx::kNullAcceleratedWidget || !gles2_)
      return;
    gfx::Size size = native_viewport_->GetSize();
    if (size.width() == 0 || size.height() == 0)
      return;
    gles2_->CreateContext(widget_, size);
  }

  virtual bool OnEvent(ui::Event* ui_event) MOJO_OVERRIDE {
    // Must not return early before updating capture.
    switch (ui_event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_TOUCH_PRESSED:
      native_viewport_->SetCapture();
      break;
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_TOUCH_RELEASED:
      native_viewport_->ReleaseCapture();
      break;
    default:
      break;
    }

    if (waiting_for_event_ack_ && IsRateLimitedEventType(ui_event))
      return false;

    pending_event_timestamp_ = ui_event->time_stamp().ToInternalValue();
    AllocationScope scope;

    Event::Builder event;
    event.set_action(ui_event->type());
    event.set_time_stamp(pending_event_timestamp_);

    if (ui_event->IsMouseEvent() || ui_event->IsTouchEvent()) {
      ui::LocatedEvent* located_event =
          static_cast<ui::LocatedEvent*>(ui_event);
      Point::Builder location;
      location.set_x(located_event->location().x());
      location.set_y(located_event->location().y());
      event.set_location(location.Finish());
    }

    if (ui_event->IsTouchEvent()) {
      ui::TouchEvent* touch_event = static_cast<ui::TouchEvent*>(ui_event);
      TouchData::Builder touch_data;
      touch_data.set_pointer_id(touch_event->touch_id());
      event.set_touch_data(touch_data.Finish());
    }

    client_->OnEvent(event.Finish());
    waiting_for_event_ack_ = true;
    return false;
  }

  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) MOJO_OVERRIDE {
    widget_ = widget;
    CreateGLES2ContextIfNeeded();
  }

  virtual void OnResized(const gfx::Size& size) MOJO_OVERRIDE {
    CreateGLES2ContextIfNeeded();
  }

  virtual void OnDestroyed() MOJO_OVERRIDE {
    // TODO(beng):
    // Destroying |gles2_| on the shell thread here hits thread checker
    // asserts. All code must stop touching the AcceleratedWidget at this
    // point as it is dead after this call stack. jamesr said we probably
    // should make our own GLSurface and simply tell it to stop touching the
    // AcceleratedWidget via Destroy() but we have no good way of doing that
    // right now given our current threading model so james' recommendation
    // was just to wait until after we move the gl service out of process.
    // gles2_.reset();
    client_->OnDestroyed();
    base::MessageLoop::current()->Quit();
  }

 private:
  NativeViewportService* service_;
  gfx::AcceleratedWidget widget_;
  scoped_ptr<services::NativeViewport> native_viewport_;
  scoped_ptr<GLES2Impl> gles2_;
  bool waiting_for_event_ack_;
  int64 pending_event_timestamp_;

  RemotePtr<NativeViewportClient> client_;
};

NativeViewportService::NativeViewportService(
    ScopedMessagePipeHandle shell_handle)
    : shell_(shell_handle.Pass(), this),
      context_(NULL) {
}

NativeViewportService::~NativeViewportService() {}

void NativeViewportService::AcceptConnection(
    ScopedMessagePipeHandle client_handle) {
  // TODO(davemoore): We need a mechanism to determine when connectors
  // go away, so we can remove viewports correctly.
  viewports_.push_back(new NativeViewportImpl(this, client_handle.Pass()));
}

}  // namespace services
}  // namespace mojo

#if defined(OS_ANDROID)

// Android will call this.
mojo::services::NativeViewportService*
    CreateNativeViewportService(mojo::ScopedMessagePipeHandle shell_handle) {
  return new mojo::services::NativeViewportService(shell_handle.Pass());
}

#else

extern "C" MOJO_NATIVE_VIEWPORT_EXPORT MojoResult MojoMain(
    const MojoHandle shell_handle) {
  base::MessageLoopForUI loop;
  mojo::services::NativeViewportService app(
      mojo::MakeScopedHandle(mojo::MessagePipeHandle(shell_handle)).Pass());
  base::MessageLoop::current()->Run();
  return MOJO_RESULT_OK;
}

#endif // !OS_ANDROID
