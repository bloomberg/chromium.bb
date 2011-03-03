// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wm_overview_snapshot.h"

#include <vector>

#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/ui/browser.h"
#include "ui/base/x/x11_util.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"

using std::vector;

#if !defined(OS_CHROMEOS)
#error This file is only meant to be compiled for ChromeOS
#endif

namespace chromeos {

WmOverviewSnapshot::WmOverviewSnapshot()
  : WidgetGtk(TYPE_WINDOW),
    snapshot_view_(NULL),
    index_(-1),
    configured_snapshot_(false) {
}

void WmOverviewSnapshot::Init(const gfx::Size& size,
                              Browser* browser,
                              int index) {
  snapshot_view_ = new views::ImageView();

  WidgetGtk::Init(NULL, gfx::Rect(size));

  SetContentsView(snapshot_view_);

  UpdateIndex(browser, index);
}


void WmOverviewSnapshot::UpdateIndex(Browser* browser, int index) {
  vector<int> params;
  params.push_back(ui::GetX11WindowFromGtkWidget(
      GTK_WIDGET(browser->window()->GetNativeHandle())));
  params.push_back(index);
  WmIpc::instance()->SetWindowType(
      GetNativeView(),
      WM_IPC_WINDOW_CHROME_TAB_SNAPSHOT,
      &params);
  index_ = index;
}

void WmOverviewSnapshot::SetImage(const SkBitmap& image) {
  CHECK(snapshot_view_) << "Init not called before setting image.";
  snapshot_view_->SetImage(image);

  // Reset the bounds to the size of the image.
  SetBounds(gfx::Rect(image.width(), image.height()));
  configured_snapshot_ = true;
}

}  // namespace chromeos
