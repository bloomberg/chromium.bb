// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SHELF_SHELF_VIEW_H_
#define MASH_SHELF_SHELF_VIEW_H_

#include <stdint.h>

#include "base/macros.h"
#include "mash/wm/public/interfaces/user_window_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_delegate.h"

namespace mojo {
class ApplicationImpl;
}

namespace views {
class LabelButton;
}

namespace mash {
namespace shelf {

// A rudimentary mash shelf, used to build up the required wm interfaces.
class ShelfView : public views::WidgetDelegateView,
                  public views::ButtonListener,
                  public mash::wm::mojom::UserWindowObserver {
 public:
  explicit ShelfView(mojo::ApplicationImpl* app);
  ~ShelfView() override;

 private:
  // Return the index in |open_window_buttons_| corresponding to |window_id|.
  size_t GetButtonIndexById(uint32_t window_id) const;

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size GetPreferredSize() const override;

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from mash::wm::mojom::UserWindowObserver:
  void OnUserWindowObserverAdded(
      mojo::Array<mash::wm::mojom::UserWindowPtr> user_windows) override;
  void OnUserWindowAdded(mash::wm::mojom::UserWindowPtr user_window) override;
  void OnUserWindowRemoved(uint32_t window_id) override;
  void OnUserWindowTitleChanged(uint32_t window_id,
                                const mojo::String& window_title) override;

  std::vector<views::LabelButton*> open_window_buttons_;
  mash::wm::mojom::UserWindowControllerPtr user_window_controller_;
  mojo::Binding<mash::wm::mojom::UserWindowObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(ShelfView);
};

}  // namespace shelf
}  // namespace mash

#endif  // MASH_SHELF_SHELF_VIEW_H_
