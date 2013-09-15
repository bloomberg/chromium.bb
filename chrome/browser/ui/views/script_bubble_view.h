// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SCRIPT_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SCRIPT_BUBBLE_VIEW_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "ui/gfx/image/image.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/link_listener.h"

namespace content {
class WebContents;
}

namespace extensions {
class ScriptBubbleController;
}

namespace views {
class ImageView;
}

// The view in the bubble that pops up from the ScriptBubbleIconView that lists
// the extensions with the activeTab permission running content scripts on the
// current page.
class ScriptBubbleView : public views::BubbleDelegateView,
                         public views::LinkListener,
                         public base::SupportsWeakPtr<ScriptBubbleView> {
 public:
  ScriptBubbleView(views::View* anchor_view,
                   content::WebContents* web_contents);
  virtual ~ScriptBubbleView();

  // views::View methods:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // LinkListener methods:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  struct ScriptEntry {
    ScriptEntry();

    std::string extension_id;
    string16 extension_name;
    views::ImageView* extension_imageview;
  };

  // views::BubbleDelegateView methods:
  virtual void Init() OVERRIDE;

  // Call when an image has finished loading.
  void OnImageLoaded(size_t index, const gfx::Image& image);

  // A helper function to get the script controller for this tab.
  extensions::ScriptBubbleController* GetScriptBubbleController();

  // The height of the bubble in pixels.
  int height_;

  // The Web Contents we're dealing with.
  content::WebContents* web_contents_;

  // A vector containing information about the scripts running on the page.
  std::vector<ScriptEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(ScriptBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SCRIPT_BUBBLE_VIEW_H_
