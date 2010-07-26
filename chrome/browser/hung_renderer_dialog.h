// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HUNG_RENDERER_DIALOG_H_
#define CHROME_BROWSER_HUNG_RENDERER_DIALOG_H_
#pragma once

class TabContents;

namespace hung_renderer_dialog {
  void ShowForTabContents(TabContents* contents);
  void HideForTabContents(TabContents* contents);
}  // namespace hung_renderer_dialog

#endif  // CHROME_BROWSER_HUNG_RENDERER_DIALOG_H_
