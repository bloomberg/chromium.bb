// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/views/views_delegate.h"

class ChromeViewsDelegate : public views::ViewsDelegate {
 public:
  ChromeViewsDelegate();
  virtual ~ChromeViewsDelegate();

  // views::ViewsDelegate:
  virtual void SaveWindowPlacement(const views::Widget* window,
                                   const std::string& window_name,
                                   const gfx::Rect& bounds,
                                   ui::WindowShowState show_state) OVERRIDE;
  virtual bool GetSavedWindowPlacement(
      const views::Widget* widget,
      const std::string& window_name,
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;
  virtual void NotifyAccessibilityEvent(
      views::View* view, ui::AXEvent event_type) OVERRIDE;
  virtual void NotifyMenuItemFocused(const base::string16& menu_name,
                                     const base::string16& menu_item_name,
                                     int item_index,
                                     int item_count,
                                     bool has_submenu) OVERRIDE;
#if defined(OS_WIN)
  virtual HICON GetDefaultWindowIcon() const OVERRIDE;
  virtual bool IsWindowInMetro(gfx::NativeWindow window) const OVERRIDE;
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
  virtual gfx::ImageSkia* GetDefaultWindowIcon() const OVERRIDE;
#endif

#if defined(USE_ASH)
  virtual views::NonClientFrameView* CreateDefaultNonClientFrameView(
      views::Widget* widget) OVERRIDE;
#endif
  virtual void AddRef() OVERRIDE;
  virtual void ReleaseRef() OVERRIDE;
  virtual void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) OVERRIDE;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  virtual bool WindowManagerProvidesTitleBar(bool maximized) OVERRIDE;
#endif
#if defined(USE_AURA)
  virtual ui::ContextFactory* GetContextFactory() OVERRIDE;
#endif
#if defined(OS_WIN)
  virtual int GetAppbarAutohideEdges(HMONITOR monitor,
                                     const base::Closure& callback) OVERRIDE;
#endif

 private:
#if defined(OS_WIN)
  typedef std::map<HMONITOR, int> AppbarAutohideEdgeMap;

  // Callback on main thread with the edges. |returned_edges| is the value that
  // was returned from the call to GetAutohideEdges() that initiated the lookup.
  void OnGotAppbarAutohideEdges(const base::Closure& callback,
                                HMONITOR monitor,
                                int returned_edges,
                                int edges);
#endif

  // Function to retrieve default opacity value mainly based on platform
  // and desktop context.
  views::Widget::InitParams::WindowOpacity GetOpacityForInitParams(
      const views::Widget::InitParams& params);

#if defined(OS_WIN)
  AppbarAutohideEdgeMap appbar_autohide_edge_map_;
  base::WeakPtrFactory<ChromeViewsDelegate> weak_factory_;
  // If true we're in the process of notifying a callback from
  // GetAutohideEdges().start a new query.
  bool in_autohide_edges_callback_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeViewsDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_
