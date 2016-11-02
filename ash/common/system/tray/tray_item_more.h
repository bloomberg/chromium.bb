// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_ITEM_MORE_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_ITEM_MORE_H_

#include <memory>

#include "ash/common/system/tray/actionable_view.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
class View;
}

namespace ash {
class SystemTrayItem;
class TrayPopupItemStyle;

// A view with a more arrow on the right edge. Clicking on the view brings up
// the detailed view of the tray-item that owns it.
class TrayItemMore : public ActionableView {
 public:
  TrayItemMore(SystemTrayItem* owner, bool show_more);
  ~TrayItemMore() override;

  void SetLabel(const base::string16& label);
  void SetImage(const gfx::ImageSkia& image_skia);
  void SetAccessibleName(const base::string16& name);

 protected:
  // Returns a style that will be applied to elements in the UpdateStyle()
  // method. e.g. changing the label's font and color. Descendants can override
  // to apply specialized configurations of the style. e.g. changing the style's
  // ColorStyle based on whether Bluetooth is enabled/disabled.
  virtual std::unique_ptr<TrayPopupItemStyle> CreateStyle() const;

  // Applies the style created from CreateStyle(). Should be called whenever any
  // input state changes that changes the style configuration created by
  // CreateStyle(). e.g. if Bluetooth is changed between enabled/disabled then
  // a differently configured style will be returned from CreateStyle() and thus
  // it will need to be applied.
  //
  // By default this will be called when OnNativeThemeChanged() is called which
  // will ensure the most up to date theme is actually applied.
  virtual void UpdateStyle();

 private:
  // Overridden from ActionableView.
  bool PerformAction(const ui::Event& event) override;

  // Overridden from views::View.
  void GetAccessibleState(ui::AXViewState* state) override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // True if |more_| should be shown.
  bool show_more_;
  views::ImageView* icon_;
  views::Label* label_;
  views::ImageView* more_;
  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(TrayItemMore);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_ITEM_MORE_H_
