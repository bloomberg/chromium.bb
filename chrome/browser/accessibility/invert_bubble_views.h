// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_INVERT_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_ACCESSIBILITY_INVERT_BUBBLE_VIEWS_H_
#pragma once

class PrefService;
class Profile;
namespace views {
class View;
}

class InvertBubble {
 public:
  static void RegisterUserPrefs(PrefService* prefs);

  // Show a bubble telling the user that they're using Windows
  // high-constrast mode with a light-on-dark scheme, so they may be
  // interested in a high-contrast Chrome extension and a dark theme.
  // Only shows the first time we encounter this condition
  // for a particular profile.
  static void MaybeShowInvertBubble(Profile* profile,
                                    views::View* anchor_view);
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_INVERT_BUBBLE_VIEWS_H_
