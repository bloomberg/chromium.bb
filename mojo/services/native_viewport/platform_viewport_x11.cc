// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/platform_viewport.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mojo/converters/input_events/mojo_extended_key_event_data.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/rect.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/x11/x11_window.h"

namespace mojo {

class PlatformViewportX11 : public PlatformViewport,
                            public ui::PlatformWindowDelegate {
 public:
  explicit PlatformViewportX11(Delegate* delegate) : delegate_(delegate) {
  }

  ~PlatformViewportX11() override {
    // Destroy the platform-window while |this| is still alive.
    platform_window_.reset();
  }

 private:
  // Overridden from PlatformViewport:
  void Init(const gfx::Rect& bounds) override {
    CHECK(!event_source_);
    CHECK(!platform_window_);

    event_source_ = ui::PlatformEventSource::CreateDefault();

    platform_window_.reset(new ui::X11Window(this));
    platform_window_->SetBounds(bounds);
  }

  void Show() override { platform_window_->Show(); }

  void Hide() override { platform_window_->Hide(); }

  void Close() override { platform_window_->Close(); }

  gfx::Size GetSize() override { return bounds_.size(); }

  void SetBounds(const gfx::Rect& bounds) override {
    platform_window_->SetBounds(bounds);
  }

  void SetCapture() override { platform_window_->SetCapture(); }

  void ReleaseCapture() override { platform_window_->ReleaseCapture(); }

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override {
    bounds_ = new_bounds;
    delegate_->OnBoundsChanged(new_bounds);
  }

  void OnDamageRect(const gfx::Rect& damaged_region) override {}

  void DispatchEvent(ui::Event* event) override {
    delegate_->OnEvent(event);

    // We want to emulate the WM_CHAR generation behaviour of Windows.
    //
    // On Linux, we've previously inserted characters by having
    // InputMethodAuraLinux take all key down events and send a character event
    // to the TextInputClient. This causes a mismatch in code that has to be
    // shared between Windows and Linux, including blink code. Now that we're
    // trying to have one way of doing things, we need to standardize on and
    // emulate Windows character events.
    //
    // This is equivalent to what we're doing in the current Linux port, but
    // done once instead of done multiple times in different places.
    if (event->type() == ui::ET_KEY_PRESSED) {
      ui::KeyEvent* key_press_event = static_cast<ui::KeyEvent*>(event);
      ui::KeyEvent char_event(key_press_event->GetCharacter(),
                              key_press_event->key_code(),
                              key_press_event->flags());

      DCHECK_EQ(key_press_event->GetCharacter(), char_event.GetCharacter());
      DCHECK_EQ(key_press_event->key_code(), char_event.key_code());
      DCHECK_EQ(key_press_event->flags(), char_event.flags());

      char_event.SetExtendedKeyEventData(scoped_ptr<ui::ExtendedKeyEventData>(
          new MojoExtendedKeyEventData(
              key_press_event->GetLocatedWindowsKeyboardCode(),
              key_press_event->GetText(),
              key_press_event->GetUnmodifiedText())));
      char_event.set_platform_keycode(key_press_event->platform_keycode());

      delegate_->OnEvent(&char_event);
    }
  }

  void OnCloseRequest() override { platform_window_->Close(); }

  void OnClosed() override { delegate_->OnDestroyed(); }

  void OnWindowStateChanged(ui::PlatformWindowState state) override {}

  void OnLostCapture() override {}

  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {
    delegate_->OnAcceleratedWidgetAvailable(widget);
  }

  void OnActivationChanged(bool active) override {}

  scoped_ptr<ui::PlatformEventSource> event_source_;
  scoped_ptr<ui::PlatformWindow> platform_window_;
  Delegate* delegate_;
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportX11);
};

// static
scoped_ptr<PlatformViewport> PlatformViewport::Create(Delegate* delegate) {
  return scoped_ptr<PlatformViewport>(new PlatformViewportX11(delegate)).Pass();
}

}  // namespace mojo
