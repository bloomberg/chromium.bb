// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/views/theme_install_bubble_view.h"
#include "grit/generated_resources.h"

namespace {

// The roundedness of the edges of our bubble.
static const int kBubbleCornerRadius = 4;

// Padding around text in popup box.
static const int kTextHorizPadding = 90;
static const int kTextVertPadding = 45;

// Multiple loads can be started at once.  Only show one bubble, and keep
// track of number of loads happening.  Close bubble when num_loads < 1.
static int num_loads_extant_ = 0;

}

ThemeInstallBubbleView::ThemeInstallBubbleView(TabContents* tab_contents)
    : popup_(NULL) {
  if (!tab_contents)
    Close();

  // Need our own copy of the "Loading..." string: http://crbug.com/24177
  text_ = l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font font(rb.GetFont(ResourceBundle::LargeFont));
  SetFont(font);

  // We can't check for the size of tab_contents before we've generated
  // the string and the font that determine the size of the bubble.
  tab_contents->GetContainerBounds(&tab_contents_bounds_);
  if (tab_contents_bounds_.height() < GetPreferredSize().height())
    Close();

  // Close when theme has been installed.
  registrar_.Add(
      this,
      NotificationType::BROWSER_THEME_CHANGED,
      NotificationService::AllSources());

  // Close when we are installing an extension, not a theme.
  registrar_.Add(
      this,
      NotificationType::NO_THEME_DETECTED,
      NotificationService::AllSources());
  registrar_.Add(
      this,
      NotificationType::EXTENSION_INSTALLED,
      NotificationService::AllSources());
  registrar_.Add(
      this,
      NotificationType::EXTENSION_INSTALL_ERROR,
      NotificationService::AllSources());
  gfx::Rect rc(0, 0, 0, 0);
  popup_ = new views::WidgetWin;
  popup_->set_window_style(WS_POPUP);
  popup_->set_window_ex_style(WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                              WS_EX_TRANSPARENT);
  popup_->SetOpacity(0xCC);
  popup_->Init(tab_contents->GetNativeView(), rc);
  popup_->SetContentsView(this);
  Reposition();
  popup_->Show();

  SchedulePaint();
}

ThemeInstallBubbleView::~ThemeInstallBubbleView() {
  num_loads_extant_ = 0;
}

gfx::Size ThemeInstallBubbleView::GetPreferredSize() {
  return gfx::Size(views::Label::GetFont().GetStringWidth(text_) +
      kTextHorizPadding,
      ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::LargeFont).height() + kTextVertPadding);
}

void ThemeInstallBubbleView::Reposition() {
  if (!popup_)
    Close();

  gfx::Size size = GetPreferredSize();
  int mid_x = tab_contents_bounds_.x() +
      (tab_contents_bounds_.right() - tab_contents_bounds_.x()) / 2;

  int x = UILayoutIsRightToLeft() ?
      mid_x + size.width() / 2 : mid_x - size.width() / 2;
  int y = static_cast<int>(tab_contents_bounds_.y() +
      (tab_contents_bounds_.bottom() - tab_contents_bounds_.y()) / 2 -
      size.height() / 2);

  popup_->MoveWindow(x, y, size.width(), size.height());
}

void ThemeInstallBubbleView::Paint(gfx::Canvas* canvas) {
  SkScalar rad[8];
  for (int i = 0; i < 8; ++i)
    rad[i] = SkIntToScalar(kBubbleCornerRadius);

  gfx::Rect popup_bounds;
  popup_->GetBounds(&popup_bounds, true);

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setColor(SK_ColorBLACK);

  int width = popup_bounds.width();
  int height = popup_bounds.height();
  SkRect rect;
  rect.set(0, 0,
           SkIntToScalar(width),
           SkIntToScalar(height));
  SkPath path;
  path.addRoundRect(rect, rad, SkPath::kCW_Direction);
  canvas->drawPath(path, paint);

  int text_width = views::Label::GetFont().GetStringWidth(text_);
  gfx::Rect body_bounds(kTextHorizPadding / 2, 0, text_width, height);
  body_bounds.set_x(MirroredLeftPointForRect(body_bounds));

  SkColor text_color = SK_ColorWHITE;
  canvas->DrawStringInt(text_, views::Label::GetFont(), text_color,
                        body_bounds.x(), body_bounds.y(), body_bounds.width(),
                        body_bounds.height());
}

void ThemeInstallBubbleView::Close() {
  --num_loads_extant_;
  if (!popup_) {
    num_loads_extant_ = 0;
    return;
  }
  if (num_loads_extant_ < 1) {
    registrar_.RemoveAll();
    popup_->Close();
  }
}

void ThemeInstallBubbleView::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  Close();
}

// static
void ThemeInstallBubbleView::Show(TabContents* tab_contents) {
  ++num_loads_extant_;
  if (num_loads_extant_ < 2)
    new ThemeInstallBubbleView(tab_contents);
}

