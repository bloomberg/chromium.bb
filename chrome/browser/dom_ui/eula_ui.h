// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_EULA_UI_H_
#define CHROME_BROWSER_DOM_UI_EULA_UI_H_

// Page to display the EULA for the internal Flash Player.
// TODO(viettrungluu): Very temporary. Remove.

#include "chrome/browser/dom_ui/dom_ui.h"

class EulaUI : public DOMUI {
 public:
  explicit EulaUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(EulaUI);
};

#endif  // CHROME_BROWSER_DOM_UI_EULA_UI_H_
