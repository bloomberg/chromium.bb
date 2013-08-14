// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/view.h"

// ContentsContainer is responsible for managing the active WebContents view.
// ContentsContainer has one child: the currently active WebContents.
class ContentsContainer : public views::View {
 public:
  explicit ContentsContainer(views::View* active_web_view);
  virtual ~ContentsContainer();

  // Sets the active top margin; the active WebView's y origin would be
  // positioned at this |margin|, causing the active WebView to be pushed down
  // vertically by |margin| pixels in the |ContentsContainer|. Returns true
  // if the margin changed and this view needs Layout().
  bool SetActiveTopMargin(int margin);

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

 private:
  views::View* active_web_view_;

  // The margin between the top and the active view. This is used to make the
  // find bar overlap the detached bookmark bar on the new tab page.
  int active_top_margin_;

  DISALLOW_COPY_AND_ASSIGN(ContentsContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
