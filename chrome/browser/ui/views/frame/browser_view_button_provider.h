// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_BUTTON_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_BUTTON_PROVIDER_H_

class BrowserActionsContainer;

namespace views {
class MenuButton;
}

// An interface implemented by a class that provides buttons that a BrowserView
// uses.
class BrowserViewButtonProvider {
 public:
  // Returns the container for extension icons.
  virtual BrowserActionsContainer* GetBrowserActionsContainer() = 0;

  // Get the app menu button for the BrowserView.
  virtual views::MenuButton* GetAppMenuButton() = 0;

  // TODO(calamity): Move other buttons and button actions into here.
 protected:
  virtual ~BrowserViewButtonProvider() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_BUTTON_PROVIDER_H_
