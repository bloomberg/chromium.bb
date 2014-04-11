// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler_delegate.h"
#include "ui/compositor/layer_owner_delegate.h"
#include "ui/views/controls/webview/webview.h"

class StatusBubbleViews;

namespace ui {
class LayerTreeOwner;
}

// ContentsWebView is used to present the WebContents of the active tab.
class ContentsWebView
    : public views::WebView,
      public ui::LayerOwnerDelegate,
      public WebContentsCloseHandlerDelegate {
 public:
  explicit ContentsWebView(content::BrowserContext* browser_context);
  virtual ~ContentsWebView();

  // Sets the status bubble, which should be repositioned every time
  // this view changes visible bounds.
  void SetStatusBubble(StatusBubbleViews* status_bubble);

  // WebView overrides:
  virtual bool NeedsNotificationWhenVisibleBoundsChange() const OVERRIDE;
  virtual void OnVisibleBoundsChanged() OVERRIDE;
  virtual void ViewHierarchyChanged(const ViewHierarchyChangedDetails& details)
      OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;

  // ui::LayerOwnerDelegate overrides:
  virtual void OnLayerRecreated(ui::Layer* old_layer,
                                ui::Layer* new_layer) OVERRIDE;

  // WebContentsCloseHandlerDelegate overrides:
  virtual void CloneWebContentsLayer() OVERRIDE;
  virtual void DestroyClonedLayer() OVERRIDE;

 private:
  StatusBubbleViews* status_bubble_;

  scoped_ptr<ui::LayerTreeOwner> cloned_layer_tree_;

  DISALLOW_COPY_AND_ASSIGN(ContentsWebView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
