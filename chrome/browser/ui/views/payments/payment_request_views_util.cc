// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"

#include "base/memory/ptr_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {

std::unique_ptr<views::View> CreatePaymentView(
    const base::string16& title, std::unique_ptr<views::View> content_view) {
  std::unique_ptr<views::View> view = base::MakeUnique<views::View>();
  view->set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));

  // Paint the sheets to layers, otherwise the MD buttons (which do paint to a
  // layer) won't do proper clipping.
  view->SetPaintToLayer(true);

  views::GridLayout* layout = new views::GridLayout(view.get());
  view->SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     1, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(new views::Label(title));

  layout->StartRow(0, 0);
  // |content_view| will be deleted when |view| is.
  layout->AddView(content_view.release());

  return view;
}

}  // namespace payments
