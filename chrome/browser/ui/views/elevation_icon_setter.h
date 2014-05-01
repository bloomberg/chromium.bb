// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ELEVATION_ICON_SETTER_H_
#define CHROME_BROWSER_UI_VIEWS_ELEVATION_ICON_SETTER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

class SkBitmap;

namespace views {
class LabelButton;
}

// On Windows, badges a button with a "UAC shield" icon to indicate that
// clicking will trigger a UAC elevation prompt.  Does nothing on other
// platforms.
class ElevationIconSetter {
 public:
  // |button| must be guaranteed to be alive throughout this class' lifetime!
  explicit ElevationIconSetter(views::LabelButton* button);
  ~ElevationIconSetter();

 private:
  void SetButtonIcon(scoped_ptr<SkBitmap> icon);

  views::LabelButton* button_;
  base::WeakPtrFactory<ElevationIconSetter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ElevationIconSetter);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ELEVATION_ICON_SETTER_H_
