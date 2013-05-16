// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace views {
class WebView;
};

// ContentsContainer is responsible for managing the active WebContents view.
// ContentsContainer has one child: the currently active WebContents.
// TODO(kuan): Remove ContentsContainer since it has only one child now -
// http://crbug.com/236587.
class ContentsContainer : public views::View {
 public:
  // Internal class name
  static const char kViewClassName[];

  explicit ContentsContainer(views::View* active);
  virtual ~ContentsContainer();

  // Makes the overlay view the active view and nulls out the old active view.
  // The caller must delete or remove the old active view separately.
  // Called after |overlay| has been reparented to |ContentsContainer|.
  void SetActive(views::WebView* overlay);

  // Sets the active top margin; the active WebView's y origin would be
  // positioned at this |margin|, causing the active WebView to be pushed down
  // vertically by |margin| pixels in the |ContentsContainer|. Returns true
  // if the margin changed and this view needs Layout().
  bool SetActiveTopMargin(int margin);

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

  // Testing interface:
  views::View* GetActiveWebViewForTest() { return active_; }

 private:
  views::View* active_;

  // The margin between the top and the active view. This is used to make the
  // overlay overlap the bookmark bar on the new tab page.
  int active_top_margin_;

  DISALLOW_COPY_AND_ASSIGN(ContentsContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
