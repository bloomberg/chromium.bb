// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ELEVATION_ICON_SETTER_H_
#define CHROME_BROWSER_UI_VIEWS_ELEVATION_ICON_SETTER_H_

namespace views {
class LabelButton;
}

// On Windows, badges |button| with a "UAC shield" icon to indicate that
// clicking will trigger a UAC elevation prompt.  Does nothing on other
// platforms.
void AddElevationIconToButton(views::LabelButton* button);

#endif  // CHROME_BROWSER_UI_VIEWS_ELEVATION_ICON_SETTER_H_
