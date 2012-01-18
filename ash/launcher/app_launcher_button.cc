// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/app_launcher_button.h"

#include <algorithm>

#include "ash/launcher/launcher_button_host.h"
#include "ui/gfx/canvas_skia.h"

namespace ash {

namespace internal {

const int kImageSize = 32;

AppLauncherButton::AppLauncherButton(views::ButtonListener* listener,
                                     LauncherButtonHost* host)
    : views::ImageButton(listener),
      host_(host) {
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
}

AppLauncherButton::~AppLauncherButton() {
}

void AppLauncherButton::SetAppImage(const SkBitmap& image) {
  if (image.empty()) {
    // TODO: need an empty image.
    SetImage(BS_NORMAL, &image);
    return;
  }
  // Resize the image maintaining our aspect ratio.
  int pref = kImageSize;
  float aspect_ratio =
      static_cast<float>(image.width()) / static_cast<float>(image.height());
  int height = pref;
  int width = static_cast<int>(aspect_ratio * height);
  if (width > pref) {
    width = pref;
    height = static_cast<int>(width / aspect_ratio);
  }
  if (width == image.width() && height == image.height()) {
    SetImage(BS_NORMAL, &image);
    return;
  }
  gfx::CanvasSkia canvas(gfx::Size(width, height), false);
  canvas.DrawBitmapInt(image, 0, 0, image.width(), image.height(),
                       0, 0, width, height, false);
  SkBitmap resized_image(canvas.ExtractBitmap());
  SetImage(BS_NORMAL, &resized_image);
}

bool AppLauncherButton::OnMousePressed(const views::MouseEvent& event) {
  ImageButton::OnMousePressed(event);
  host_->MousePressedOnButton(this, event);
  return true;
}

void AppLauncherButton::OnMouseReleased(const views::MouseEvent& event) {
  host_->MouseReleasedOnButton(this, false);
  ImageButton::OnMouseReleased(event);
}

void AppLauncherButton::OnMouseCaptureLost() {
  host_->MouseReleasedOnButton(this, true);
  ImageButton::OnMouseCaptureLost();
}

bool AppLauncherButton::OnMouseDragged(const views::MouseEvent& event) {
  ImageButton::OnMouseDragged(event);
  host_->MouseDraggedOnButton(this, event);
  return true;
}

void AppLauncherButton::OnMouseExited(const views::MouseEvent& event) {
  ImageButton::OnMouseExited(event);
  host_->MouseExitedButton(this);
}

}  // namespace internal
}  // namespace ash
