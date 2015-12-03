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

  // Show surface as a toplevel window.
  void SetToplevel();

  // Maximize or show surface as a maximized window.
  void SetMaximized();

  // Fullscreen or show surface as a fullscreen window.
  void SetFullscreen();

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
  scoped_ptr<views::Widget> widget_;
  Surface* surface_;
  base::string16 title_;
  std::string application_id_;

  DISALLOW_COPY_AND_ASSIGN(ShellSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_H_
