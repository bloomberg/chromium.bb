// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/launcher/alternate_app_list_button.h"

#include "ash/ash_switches.h"
#include "ash/launcher/launcher_button_host.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {
namespace internal {
namespace {
const int kImagePaddingFromShelf = 5;
}  // namespace

// static
const int AlternateAppListButton::kImageBoundsSize = 7;


AlternateAppListButton::AlternateAppListButton(views::ButtonListener* listener,
                                               LauncherButtonHost* host,
                                               ShelfWidget* shelf_widget)
    : views::ImageButton(listener),
      host_(host),
      shelf_widget_(shelf_widget) {
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_AURA_APP_LIST_TITLE));
  SetSize(gfx::Size(ShelfLayoutManager::kShelfSize,
                    ShelfLayoutManager::kShelfSize));
}

AlternateAppListButton::~AlternateAppListButton() {
}

bool AlternateAppListButton::OnMousePressed(const ui::MouseEvent& event) {
  ImageButton::OnMousePressed(event);
  host_->PointerPressedOnButton(this, LauncherButtonHost::MOUSE, event);
  return true;
}

void AlternateAppListButton::OnMouseReleased(const ui::MouseEvent& event) {
  ImageButton::OnMouseReleased(event);
  host_->PointerReleasedOnButton(this, LauncherButtonHost::MOUSE, false);
}

void AlternateAppListButton::OnMouseCaptureLost() {
  host_->PointerReleasedOnButton(this, LauncherButtonHost::MOUSE, true);
  ImageButton::OnMouseCaptureLost();
}

bool AlternateAppListButton::OnMouseDragged(const ui::MouseEvent& event) {
  ImageButton::OnMouseDragged(event);
  host_->PointerDraggedOnButton(this, LauncherButtonHost::MOUSE, event);
  return true;
}

void AlternateAppListButton::OnMouseMoved(const ui::MouseEvent& event) {
  ImageButton::OnMouseMoved(event);
  host_->MouseMovedOverButton(this);
}

void AlternateAppListButton::OnMouseEntered(const ui::MouseEvent& event) {
  ImageButton::OnMouseEntered(event);
  host_->MouseEnteredButton(this);
}

void AlternateAppListButton::OnMouseExited(const ui::MouseEvent& event) {
  ImageButton::OnMouseExited(event);
  host_->MouseExitedButton(this);
}

void AlternateAppListButton::OnPaint(gfx::Canvas* canvas) {
  // Call the base class first to paint any background/borders.
  View::OnPaint(canvas);

  int background_image_id = 0;
  if (Shell::GetInstance()->GetAppListTargetVisibility()) {
    background_image_id = IDR_AURA_NOTIFICATION_BACKGROUND_PRESSED;
  } else {
    if (shelf_widget_->GetDimsShelf())
      background_image_id = IDR_AURA_NOTIFICATION_BACKGROUND_ON_BLACK;
    else
      background_image_id = IDR_AURA_NOTIFICATION_BACKGROUND_NORMAL;
  }
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* background_image =
      rb.GetImageNamed(background_image_id).ToImageSkia();
  const gfx::ImageSkia* forground_image =
      rb.GetImageNamed(IDR_AURA_LAUNCHER_ICON_APPLIST_ALTERNATE).ToImageSkia();

  gfx::Rect contents_bounds = GetContentsBounds();
  gfx::Rect background_bounds, forground_bounds;

  ShelfAlignment alignment = shelf_widget_->GetAlignment();
  background_bounds.set_size(background_image->size());
  if (alignment == SHELF_ALIGNMENT_LEFT) {
    background_bounds.set_x(contents_bounds.width() -
        kImagePaddingFromShelf - background_image->width());
    background_bounds.set_y(contents_bounds.y() +
        (contents_bounds.height() - background_image->height()) / 2);
  } else if(alignment == SHELF_ALIGNMENT_RIGHT) {
    background_bounds.set_x(kImagePaddingFromShelf);
    background_bounds.set_y(contents_bounds.y() +
        (contents_bounds.height() - background_image->height()) / 2);
  } else {
    background_bounds.set_y(kImagePaddingFromShelf);
    background_bounds.set_x(contents_bounds.x() +
        (contents_bounds.width() - background_image->width()) / 2);
  }

  forground_bounds.set_size(forground_image->size());
  forground_bounds.set_x(background_bounds.x() +
      std::max(0,
          (background_bounds.width() - forground_bounds.width()) / 2));
  forground_bounds.set_y(background_bounds.y() +
      std::max(0,
          (background_bounds.height() - forground_bounds.height()) / 2));

  canvas->DrawImageInt(*background_image,
                       background_bounds.x(),
                       background_bounds.y());
  canvas->DrawImageInt(*forground_image,
                       forground_bounds.x(),
                       forground_bounds.y());

  OnPaintFocusBorder(canvas);
}

void AlternateAppListButton::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = host_->GetAccessibleName(this);
}

}  // namespace internal
}  // namespace ash
