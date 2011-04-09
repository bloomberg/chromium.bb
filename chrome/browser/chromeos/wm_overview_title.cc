// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wm_overview_title.h"

#include <vector>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/drop_shadow_label.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/chromeos/wm_overview_snapshot.h"
#include "chrome/browser/ui/browser_window.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/x/x11_util.h"
#include "views/border.h"
#include "views/layout/grid_layout.h"
#include "views/view.h"

using std::vector;
using views::ColumnSet;
using views::GridLayout;
using views::View;
using gfx::Font;

#if !defined(OS_CHROMEOS)
#error This file is only meant to be compiled for ChromeOS
#endif

namespace chromeos {

// The padding between the title and the URL, in pixels.
static const int kVerticalPadding = 2;

// This is the size (in pixels) of the drop shadow on the title and url text.
static const int kDropShadowSize = 2;

namespace {
// Finds a font based on the base font that is no taller than the
// given value (well, unless the font at size 1 is taller than the
// given value).
Font FindFontThisHigh(int pixels, Font base) {
  Font font(base.GetFontName(), 1);
  Font last_font = font;
  while (font.GetHeight() < pixels) {
    last_font = font;
    font = font.DeriveFont(1, Font::BOLD);
  }
  return last_font;
}
}  // Anonymous namespace

WmOverviewTitle::WmOverviewTitle()
  : WidgetGtk(TYPE_WINDOW),
    title_label_(NULL),
    url_label_(NULL) {
}

void WmOverviewTitle::Init(const gfx::Size& size,
                           WmOverviewSnapshot* snapshot) {
  MakeTransparent();

  title_label_ = new DropShadowLabel();
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetColor(SkColorSetARGB(0xFF, 0xFF, 0xFF, 0xFF));
  title_label_->SetDropShadowSize(kDropShadowSize);
  title_label_->SetFont(FindFontThisHigh(16, title_label_->font()));

  url_label_ = new DropShadowLabel();
  url_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  url_label_->SetColor(SkColorSetARGB(0x80, 0xFF, 0xFF, 0xFF));
  url_label_->SetDropShadowSize(kDropShadowSize);
  url_label_->SetFont(FindFontThisHigh(14, title_label_->font()));

  // Create a new view to be the host for the grid.
  View* view = new View();
  GridLayout* layout = new GridLayout(view);
  view->SetLayoutManager(layout);

  int title_cs_id = 0;
  ColumnSet* columns = layout->AddColumnSet(title_cs_id);
  columns->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                     GridLayout::FIXED, size.width(), 0);

  layout->StartRow(0, title_cs_id);
  layout->AddView(title_label_);
  layout->StartRowWithPadding(1, title_cs_id, 0, kVerticalPadding);
  layout->AddView(url_label_);

  // Realize the widget.
  WidgetGtk::Init(NULL, gfx::Rect(size));

  // Make the view the contents view for this widget.
  SetContentsView(view);

  // Set the window type
  vector<int> params;
  params.push_back(ui::GetX11WindowFromGtkWidget(
      GTK_WIDGET(snapshot->GetNativeView())));
  WmIpc::instance()->SetWindowType(
      GetNativeView(),
      WM_IPC_WINDOW_CHROME_TAB_TITLE,
      &params);
}

void WmOverviewTitle::SetTitle(const string16& title) {
  title_label_->SetText(UTF16ToWide(title));
}

void WmOverviewTitle::SetUrl(const GURL& url) {
  url_label_->SetURL(url);
}
}  // namespace chromeos
