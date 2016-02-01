// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_H_
#define COMPONENTS_EXO_SHELL_SURFACE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "ui/views/widget/widget_delegate.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace exo {
class Surface;

// This class provides functions for treating a surfaces like toplevel,
// fullscreen or popup widgets, move, resize or maximize them, associate
// metadata like title and class, etc.
class ShellSurface : public SurfaceDelegate,
                     public SurfaceObserver,
                     public views::WidgetDelegate,
                     public views::View {
 public:
  explicit ShellSurface(Surface* surface);
  ~ShellSurface() override;

  // Set the callback to run when the user wants the shell surface to be closed.
  // The receiver can chose to not close the window on this signal.
  void set_close_callback(const base::Closure& close_callback) {
    close_callback_ = close_callback;
  }

  // Set the callback to run when the surface is destroyed.
  void set_surface_destroyed_callback(
      const base::Closure& surface_destroyed_callback) {
    surface_destroyed_callback_ = surface_destroyed_callback;
  }

  // Set the callback to run when the client is asked to resize the surface.
  // The size is a hint, in the sense that the client is free to ignore it if
  // it doesn't resize, pick a smaller size (to satisfy aspect ratio or resize
  // in steps of NxM pixels).
  using ConfigureCallback = base::Callback<void(const gfx::Size& size)>;
  void set_configure_callback(const ConfigureCallback& configure_callback) {
    configure_callback_ = configure_callback;
  }

  // Maximizes the shell surface.
  void Maximize();

  // Restore the shell surface.
  void Restore();

  // Set fullscreen state for shell surface.
  void SetFullscreen(bool fullscreen);

  // Set title for surface.
  void SetTitle(const base::string16& title);

  // Sets the application ID for the window. The application ID identifies the
  // general class of applications to which the window belongs.
  static void SetApplicationId(aura::Window* window,
                               std::string* application_id);
  static const std::string GetApplicationId(aura::Window* window);

  // Set application id for surface.
  void SetApplicationId(const std::string& application_id);

  // Start an interactive move of surface.
  void Move();

  // Signal a request to close the window. It is up to the implementation to
  // actually decide to do so though.
  void Close();

  // Set geometry for surface. The geometry represents the "visible bounds"
  // for the surface from the user's perspective.
  void SetGeometry(const gfx::Rect& geometry);

  // Returns a trace value representing the state of the surface.
  scoped_refptr<base::trace_event::TracedValue> AsTracedValue() const;

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsSurfaceSynchronized() const override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override;

 private:
  // Creates the |widget_| for |surface_|.
  void CreateShellSurfaceWidget();

  scoped_ptr<views::Widget> widget_;
  Surface* surface_;
  base::string16 title_;
  std::string application_id_;
  gfx::Rect geometry_;
  base::Closure close_callback_;
  base::Closure surface_destroyed_callback_;
  ConfigureCallback configure_callback_;

  DISALLOW_COPY_AND_ASSIGN(ShellSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_H_
