// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_service.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/gles2/command_buffer_impl.h"
#include "mojo/services/native_viewport/geometry_conversions.h"
#include "mojo/services/native_viewport/native_viewport.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
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

class NativeViewportImpl
    : public Service<mojo::NativeViewport, NativeViewportImpl, shell::Context>,
      public NativeViewportDelegate {
 public:
  NativeViewportImpl()
      : widget_(gfx::kNullAcceleratedWidget),
        waiting_for_event_ack_(false) {}
  virtual ~NativeViewportImpl() {}

  virtual void Create(const Rect& bounds) OVERRIDE {
    native_viewport_ =
        services::NativeViewport::Create(context(), this);
    native_viewport_->Init(bounds);
    client()->OnCreated();
    OnBoundsChanged(bounds);
  }

  virtual void Show() OVERRIDE {
    native_viewport_->Show();
  }

  virtual void Hide() OVERRIDE {
    native_viewport_->Hide();
  }

  virtual void Close() OVERRIDE {
    command_buffer_.reset();
    DCHECK(native_viewport_);
    native_viewport_->Close();
  }

  virtual void SetBounds(const Rect& bounds) OVERRIDE {
    gfx::Rect gfx_bounds(bounds.position().x(), bounds.position().y(),
                         bounds.size().width(), bounds.size().height());
    native_viewport_->SetBounds(gfx_bounds);
  }

  virtual void CreateGLES2Context(ScopedMessagePipeHandle client_handle)
      OVERRIDE {
    if (command_buffer_ || command_buffer_handle_.is_valid()) {
      LOG(ERROR) << "Can't create multiple contexts on a NativeViewport";
      return;
    }

    // TODO(darin):
    // CreateGLES2Context should accept a ScopedCommandBufferClientHandle once
    // it is possible to import interface definitions from another module.  For
    // now, we just kludge it.
    command_buffer_handle_.reset(
        InterfaceHandle<CommandBufferClient>(client_handle.release().value()));

    CreateCommandBufferIfNeeded();
  }

  void AckEvent() {
    waiting_for_event_ack_ = false;
  }

  void CreateCommandBufferIfNeeded() {
    if (!command_buffer_handle_.is_valid())
      return;
    DCHECK(!command_buffer_.get());
    if (widget_ == gfx::kNullAcceleratedWidget)
      return;
    gfx::Size size = native_viewport_->GetSize();
    if (size.IsEmpty())
      return;
    command_buffer_.reset(new CommandBufferImpl(
        command_buffer_handle_.Pass(), widget_, native_viewport_->GetSize()));
  }

  virtual bool OnEvent(ui::Event* ui_event) OVERRIDE {
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

    AllocationScope scope;

    Event::Builder event;
    event.set_action(ui_event->type());
    event.set_flags(ui_event->flags());
    event.set_time_stamp(ui_event->time_stamp().ToInternalValue());

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
    } else if (ui_event->IsKeyEvent()) {
      ui::KeyEvent* key_event = static_cast<ui::KeyEvent*>(ui_event);
      KeyData::Builder key_data;
      key_data.set_key_code(key_event->key_code());
      key_data.set_is_char(key_event->is_char());
      event.set_key_data(key_data.Finish());
    }

    client()->OnEvent(event.Finish(),
                      base::Bind(&NativeViewportImpl::AckEvent,
                                 base::Unretained(this)));
    waiting_for_event_ack_ = true;
    return false;
  }

  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) OVERRIDE {
    widget_ = widget;
    CreateCommandBufferIfNeeded();
  }

  virtual void OnBoundsChanged(const gfx::Rect& bounds) OVERRIDE {
    CreateCommandBufferIfNeeded();
    AllocationScope scope;
    client()->OnBoundsChanged(bounds);
  }

  virtual void OnDestroyed() OVERRIDE {
    command_buffer_.reset();
    client()->OnDestroyed();
    base::MessageLoop::current()->Quit();
  }

 private:
  gfx::AcceleratedWidget widget_;
  scoped_ptr<services::NativeViewport> native_viewport_;
  ScopedCommandBufferClientHandle command_buffer_handle_;
  scoped_ptr<CommandBufferImpl> command_buffer_;
  bool waiting_for_event_ack_;
};

}  // namespace services
}  // namespace mojo


MOJO_NATIVE_VIEWPORT_EXPORT mojo::Application*
    CreateNativeViewportService(mojo::shell::Context* context,
                                mojo::ScopedShellHandle shell_handle) {
  mojo::Application* app = new mojo::Application(shell_handle.Pass());
  app->AddServiceFactory(
    new mojo::ServiceFactory<mojo::services::NativeViewportImpl,
                             mojo::shell::Context>(context));
  return app;
}

