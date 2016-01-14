// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/shelf/shelf_view.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"

namespace mash {
namespace shelf {

enum ShelfButtonID {
  SHELF_BUTTON_VIEWS_EXAMPLES,
  SHELF_BUTTON_TASK_VIEWER,
};

ShelfView::ShelfView(mojo::ApplicationImpl* app) : app_(app), binding_(this) {
  app->ConnectToService("mojo:desktop_wm", &user_window_controller_);

  mash::wm::mojom::UserWindowObserverPtr observer;
  mojo::InterfaceRequest<mash::wm::mojom::UserWindowObserver> request =
      mojo::GetProxy(&observer);
  user_window_controller_->AddUserWindowObserver(std::move(observer));
  binding_.Bind(std::move(request));

  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

  views::LabelButton* views_examples =
      new views::LabelButton(this, base::ASCIIToUTF16("Views Examples"));
  views_examples->set_tag(SHELF_BUTTON_VIEWS_EXAMPLES);
  AddChildView(views_examples);

  views::LabelButton* task_viewer =
      new views::LabelButton(this, base::ASCIIToUTF16("Task Viewer"));
  task_viewer->set_tag(SHELF_BUTTON_TASK_VIEWER);
  AddChildView(task_viewer);
}

ShelfView::~ShelfView() {}

void ShelfView::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(GetLocalBounds(), SK_ColorYELLOW);
  views::View::OnPaint(canvas);
}

gfx::Size ShelfView::GetPreferredSize() const {
  return gfx::Size(1, 48);
}

views::View* ShelfView::GetContentsView() {
  return this;
}

void ShelfView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender->tag() == SHELF_BUTTON_VIEWS_EXAMPLES)
    app_->ConnectToApplication("mojo:views_examples");
  else if (sender->tag() == SHELF_BUTTON_TASK_VIEWER)
    app_->ConnectToApplication("mojo:task_viewer");
  else
    user_window_controller_->FocusUserWindow(sender->tag());
}

void ShelfView::OnUserWindowObserverAdded(mojo::Array<uint32_t> window_ids) {
  for (size_t i = 0; i < window_ids.size(); ++i)
    OnUserWindowAdded(window_ids[i]);
}

void ShelfView::OnUserWindowAdded(uint32_t window_id) {
  // TODO(msw): Get the actual window title and icon.
  views::LabelButton* open_window_button = new views::LabelButton(
      this, base::ASCIIToUTF16(base::StringPrintf("Window %d", window_id)));
  open_window_button->set_tag(window_id);
  open_window_buttons_.push_back(open_window_button);
  AddChildView(open_window_button);
  Layout();
  SchedulePaint();
}

void ShelfView::OnUserWindowRemoved(uint32_t window_id) {
  for (size_t i = 0; i < open_window_buttons_.size(); ++i) {
    if (static_cast<uint32_t>(open_window_buttons_[i]->tag()) == window_id) {
      views::LabelButton* button = open_window_buttons_[i];
      open_window_buttons_.erase(open_window_buttons_.begin() + i);
      RemoveChildView(button);
      delete button;
      return;
    }
  }
}

}  // namespace shelf
}  // namespace mash
