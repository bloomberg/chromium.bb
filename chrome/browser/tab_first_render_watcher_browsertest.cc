// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/tab_first_render_watcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace {

views::Widget* CreateWindowForContents(views::View* contents) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  views::Widget* widget = new views::Widget;
  widget->Init(widget_params);
  widget->SetContentsView(contents);

  return widget;
}

}  // namespace

class TabFirstRenderWatcherTest : public InProcessBrowserTest,
                                  public TabFirstRenderWatcher::Delegate {
 public:
  TabFirstRenderWatcherTest()
      : host_created_(false),
        main_frame_loaded_(false),
        main_frame_rendered_(false) {
  }

  // TabFirstRenderWatcher::Delegate implementation.
  virtual void OnRenderHostCreated(RenderViewHost* host) OVERRIDE {
    host_created_ = true;
  }

  virtual void OnTabMainFrameLoaded() OVERRIDE {
    main_frame_loaded_ = true;
    MessageLoop::current()->Quit();
  }

  virtual void OnTabMainFrameFirstRender() OVERRIDE {
    main_frame_rendered_ = true;
    MessageLoop::current()->Quit();
  }

 protected:
  bool host_created_;
  bool main_frame_loaded_;
  bool main_frame_rendered_;
};

// Migrated from HtmlDialogBrowserTest.TestStateTransition, which times out
// about 5~10% of runs. See crbug.com/86059.
IN_PROC_BROWSER_TEST_F(TabFirstRenderWatcherTest,
                       DISABLED_TestStateTransition) {
  DOMView* dom_view = new DOMView;
  dom_view->Init(browser()->profile(), NULL);
  CreateWindowForContents(dom_view);
  dom_view->GetWidget()->Show();

  scoped_ptr<TabFirstRenderWatcher> watcher(
      new TabFirstRenderWatcher(dom_view->dom_contents()->tab_contents(),
                                this));

  EXPECT_FALSE(host_created_);
  EXPECT_FALSE(main_frame_loaded_);
  EXPECT_FALSE(main_frame_rendered_);

  dom_view->LoadURL(GURL(chrome::kChromeUIChromeURLsURL));
  EXPECT_TRUE(host_created_);

  // OnTabMainFrameLoaded() will Quit().
  MessageLoopForUI::current()->Run();
  EXPECT_TRUE(main_frame_loaded_);

  // OnTabMainFrameFirstRender() will Quit().
  MessageLoopForUI::current()->Run();
  EXPECT_TRUE(main_frame_rendered_);

  dom_view->GetWidget()->Close();
}
