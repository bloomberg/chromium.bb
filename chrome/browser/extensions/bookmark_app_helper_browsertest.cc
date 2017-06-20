// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_helper.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace extensions {
namespace {

content::RenderWidgetHost* GetActiveRenderWidgetHost(Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetRenderWidgetHostView()
      ->GetRenderWidgetHost();
}

// Extends BookmarkAppHelper to see the call to OnIconsDownloaded.
class TestBookmarkAppHelper : public BookmarkAppHelper {
 public:
  TestBookmarkAppHelper(Profile* profile,
                        WebApplicationInfo web_app_info,
                        content::WebContents* contents,
                        base::Closure on_icons_downloaded_closure)
      : BookmarkAppHelper(profile, web_app_info, contents),
        on_icons_downloaded_closure_(on_icons_downloaded_closure) {}

  // TestBookmarkAppHelper:
  void OnIconsDownloaded(
      bool success,
      const std::map<GURL, std::vector<SkBitmap>>& bitmaps) override {
    on_icons_downloaded_closure_.Run();
    BookmarkAppHelper::OnIconsDownloaded(success, bitmaps);
  }

 private:
  base::Closure on_icons_downloaded_closure_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppHelper);
};

// Intercepts the ChromeFrameHostMsg_DidGetWebApplicationInfo that would usually
// get sent to extensions::TabHelper to create a BookmarkAppHelper that lets us
// detect when icons are downloaded and the dialog is ready to show.
class WebAppReadyMsgWatcher : public content::BrowserMessageFilter {
 public:
  explicit WebAppReadyMsgWatcher(Browser* browser)
      : BrowserMessageFilter(ChromeMsgStart), browser_(browser) {}

  content::WebContents* web_contents() {
    return browser_->tab_strip_model()->GetActiveWebContents();
  }

  void OnDidGetWebApplicationInfo(const WebApplicationInfo& const_info) {
    WebApplicationInfo info = const_info;
    // Mimic extensions::TabHelper for fields missing from the manifest.
    if (info.app_url.is_empty())
      info.app_url = web_contents()->GetURL();
    if (info.title.empty())
      info.title = web_contents()->GetTitle();
    if (info.title.empty())
      info.title = base::UTF8ToUTF16(info.app_url.spec());

    bookmark_app_helper_ = base::MakeUnique<TestBookmarkAppHelper>(
        browser_->profile(), info, web_contents(), quit_closure_);
    bookmark_app_helper_->Create(
        base::Bind(&WebAppReadyMsgWatcher::FinishCreateBookmarkApp, this));
  }

  void FinishCreateBookmarkApp(const Extension* extension,
                               const WebApplicationInfo& web_app_info) {
    // ~WebAppReadyMsgWatcher() is called on the IO thread, but
    // |bookmark_app_helper_| must be destroyed on the UI thread.
    bookmark_app_helper_.reset();
  }

  void Wait() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // BrowserMessageFilter:
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override {
    if (message.type() == ChromeFrameHostMsg_DidGetWebApplicationInfo::ID)
      *thread = content::BrowserThread::UI;
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(WebAppReadyMsgWatcher, message)
      IPC_MESSAGE_HANDLER(ChromeFrameHostMsg_DidGetWebApplicationInfo,
                          OnDidGetWebApplicationInfo)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

 private:
  ~WebAppReadyMsgWatcher() override {}

  Browser* browser_;
  base::Closure quit_closure_;
  std::unique_ptr<TestBookmarkAppHelper> bookmark_app_helper_;

  DISALLOW_COPY_AND_ASSIGN(WebAppReadyMsgWatcher);
};

}  // namespace

class BookmarkAppHelperTest : public DialogBrowserTest {
 public:
  BookmarkAppHelperTest() {}

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    ASSERT_TRUE(embedded_test_server()->Start());
    AddTabAtIndex(
        1,
        GURL(embedded_test_server()->GetURL("/favicon/page_with_favicon.html")),
        ui::PAGE_TRANSITION_LINK);

    scoped_refptr<WebAppReadyMsgWatcher> filter =
        new WebAppReadyMsgWatcher(browser());
    GetActiveRenderWidgetHost(browser())->GetProcess()->AddFilter(filter.get());
    chrome::ExecuteCommand(browser(), IDC_CREATE_HOSTED_APP);
    filter->Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppHelperTest);
};

IN_PROC_BROWSER_TEST_F(BookmarkAppHelperTest, InvokeDialog_create) {
  RunDialog();
}

}  // namespace extensions
