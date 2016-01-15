// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/shelf/shelf_view.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"

namespace mash {
namespace shelf {

ShelfView::ShelfView(mojo::ApplicationImpl* app) : binding_(this) {
  app->ConnectToService("mojo:desktop_wm", &user_window_controller_);

  mash::wm::mojom::UserWindowObserverPtr observer;
  mojo::InterfaceRequest<mash::wm::mojom::UserWindowObserver> request =
      mojo::GetProxy(&observer);
  user_window_controller_->AddUserWindowObserver(std::move(observer));
  binding_.Bind(std::move(request));

  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
}

ShelfView::~ShelfView() {}

size_t ShelfView::GetButtonIndexById(uint32_t window_id) const {
  for (size_t i = 0; i < open_window_buttons_.size(); ++i)
    if (static_cast<uint32_t>(open_window_buttons_[i]->tag()) == window_id)
      return i;
  return open_window_buttons_.size();
}

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
  user_window_controller_->FocusUserWindow(sender->tag());
}

void ShelfView::OnUserWindowObserverAdded(
    mojo::Array<mash::wm::mojom::UserWindowPtr> user_windows) {
  for (size_t i = 0; i < user_windows.size(); ++i)
    OnUserWindowAdded(std::move(user_windows[i]));
}

void ShelfView::OnUserWindowAdded(mash::wm::mojom::UserWindowPtr user_window) {
  views::LabelButton* open_window_button = new views::LabelButton(
      this, user_window->window_title.To<base::string16>());
  open_window_button->set_tag(user_window->window_id);
  open_window_buttons_.push_back(open_window_button);
  AddChildView(open_window_button);
  Layout();
  SchedulePaint();
}

void ShelfView::OnUserWindowRemoved(uint32_t window_id) {
  const size_t index = GetButtonIndexById(window_id);
  if (index >= open_window_buttons_.size())
    return;

  views::LabelButton* button = open_window_buttons_[index];
  open_window_buttons_.erase(open_window_buttons_.begin() + index);
  RemoveChildView(button);
  delete button;
  Layout();
  SchedulePaint();
}

void ShelfView::OnUserWindowTitleChanged(uint32_t window_id,
                                         const mojo::String& window_title) {
  const size_t index = GetButtonIndexById(window_id);
  if (index >= open_window_buttons_.size())
    return;

  open_window_buttons_[index]->SetText(window_title.To<base::string16>());
  open_window_buttons_[index]->SetMinSize(gfx::Size());
  Layout();
  SchedulePaint();
}

}  // namespace shelf
}  // namespace mash
