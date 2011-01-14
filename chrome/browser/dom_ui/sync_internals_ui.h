// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_SYNC_INTERNALS_UI_H_
#define CHROME_BROWSER_DOM_UI_SYNC_INTERNALS_UI_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/dom_ui/dom_ui.h"

class SyncInternalsUI : public DOMUI {
 public:
  explicit SyncInternalsUI(TabContents* contents);
  virtual ~SyncInternalsUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncInternalsUI);
};

#endif  // CHROME_BROWSER_DOM_UI_SYNC_INTERNALS_UI_H_
