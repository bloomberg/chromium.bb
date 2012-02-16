// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_PANEL_VIEW_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_PANEL_VIEW_AURA_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget_delegate.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

namespace internal {
class PanelHost;
}

///////////////////////////////////////////////////////////////////////////////
// PanelViewAura is used to display HTML in a Panel window.
//
class PanelViewAura : public views::NativeViewHost,
                      public views::WidgetDelegate {
 public:
  explicit PanelViewAura(const std::string& title);
  virtual ~PanelViewAura();

  views::Widget* Init(Profile* profile,
                      const GURL& url,
                      const gfx::Rect& bounds);

  // Returns the WebContents associated with this panel.
  content::WebContents* WebContents();

  // Close the panel window.
  void CloseView();

  // Set the preferred size of the contents.
  void SetContentPreferredSize(const gfx::Size& size);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual bool CanResize() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

 private:
  std::string title_;
  gfx::Size preferred_size_;
  // Owned internal host class implementing WebContents and Extension Delegates.
  scoped_ptr<internal::PanelHost> host_;
  // Unowned pointer to the widget.
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(PanelViewAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_PANEL_VIEW_AURA_H_
