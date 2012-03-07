// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/tab_render_watcher.h"
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

class TabRenderWatcherTest : public InProcessBrowserTest,
                             public TabRenderWatcher::Delegate {
 public:
  TabRenderWatcherTest()
      : host_created_(false),
        main_frame_loaded_(false),
        main_frame_rendered_(false) {
  }

  // TabRenderWatcher::Delegate implementation.
  virtual void OnRenderHostCreated(content::RenderViewHost* host) OVERRIDE {
    host_created_ = true;
  }

  virtual void OnTabMainFrameLoaded() OVERRIDE {
    main_frame_loaded_ = true;
    MessageLoop::current()->Quit();
  }

  virtual void OnTabMainFrameRender() OVERRIDE {
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
IN_PROC_BROWSER_TEST_F(TabRenderWatcherTest, DISABLED_TestStateTransition) {
  DOMView* dom_view = new DOMView;
  dom_view->Init(browser()->profile(), NULL);
  CreateWindowForContents(dom_view);
  dom_view->GetWidget()->Show();

  scoped_ptr<TabRenderWatcher> watcher(
      new TabRenderWatcher(dom_view->dom_contents()->web_contents(), this));

  EXPECT_FALSE(host_created_);
  EXPECT_FALSE(main_frame_loaded_);
  EXPECT_FALSE(main_frame_rendered_);

  dom_view->LoadURL(GURL(chrome::kChromeUIChromeURLsURL));
  EXPECT_TRUE(host_created_);

  // OnTabMainFrameLoaded() will Quit().
  MessageLoopForUI::current()->Run();
  EXPECT_TRUE(main_frame_loaded_);

  // OnTabMainFrameRender() will Quit().
  MessageLoopForUI::current()->Run();
  EXPECT_TRUE(main_frame_rendered_);

  dom_view->GetWidget()->Close();
}
