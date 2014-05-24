// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/placeholder.h"

#include "athena/screen/public/screen_manager.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

void CreateTestWindows() {
  const int kAppWindowBackgroundColor = 0xFFDDDDDD;
  views::Widget* test_app_widget = new views::Widget;
  // Athena doesn't have frame yet.
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.context = athena::ScreenManager::Get()->GetContext();
  test_app_widget->Init(params);
  views::Label* label = new views::Label;
  label->SetText(base::ASCIIToUTF16("AppWindow"));
  label->set_background(
      views::Background::CreateSolidBackground(kAppWindowBackgroundColor));
  test_app_widget->SetContentsView(label);
  test_app_widget->Show();
}

void SetupBackgroundImage() {
  gfx::Size size(200, 200);
  gfx::Canvas canvas(size, 1.0f, true);
  scoped_ptr<views::Painter> painter(
      views::Painter::CreateVerticalGradient(SK_ColorBLUE, SK_ColorCYAN));
  painter->Paint(&canvas, size);
  athena::ScreenManager::Get()->SetBackgroundImage(
      gfx::ImageSkia(canvas.ExtractImageRep()));
}
