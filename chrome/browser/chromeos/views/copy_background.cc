// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/views/copy_background.h"

#include "base/logging.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace chromeos {

CopyBackground::CopyBackground(views::View* copy_from)
    : background_owner_(copy_from) {
  DCHECK(background_owner_);
  DCHECK(background_owner_->background());
}

void CopyBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  const Background* background = background_owner_->background();
  DCHECK(background);
  gfx::Point origin(0, 0);
  views::View::ConvertPointToView(view, background_owner_, &origin);
  canvas->Save();
  // Move the origin and paint as if it's paint onto the owner.
  canvas->Translate(gfx::Point().Subtract(origin));
  background->Paint(canvas, background_owner_);
  canvas->Restore();
}

}  // namespace chromeos
