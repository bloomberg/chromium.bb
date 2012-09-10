// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/page_action_controller.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/value_builder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"

using content::BrowserThread;

namespace extensions {
namespace {

class PageActionControllerTest : public TabContentsTestHarness {
 public:
  PageActionControllerTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()) {}

  virtual void SetUp() OVERRIDE {
    TabContentsTestHarness::SetUp();
    // Create an ExtensionService so the PageActionController can find its
    // extensions.
    CommandLine command_line(CommandLine::NO_PROGRAM);
    extension_service_ =
        static_cast<TestExtensionSystem*>(
            ExtensionSystem::Get(tab_contents()->profile()))->
        CreateExtensionService(
            &command_line, FilePath(), false);
  }

 protected:
  int tab_id() {
    return SessionID::IdForTab(web_contents());
  }

  ExtensionService* extension_service_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

TEST_F(PageActionControllerTest, NavigationClearsState) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "Extension with page action")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("permissions", ListBuilder()
                        .Append("tabs"))
                   .Set("page_action", DictionaryBuilder()
                        .Set("default_title", "Hello")))
      .Build();
  extension_service_->AddExtension(extension);

  NavigateAndCommit(GURL("http://www.google.com"));

  extension->page_action()->SetTitle(tab_id(), "Goodbye");
  extension->page_action()->SetPopupUrl(
      tab_id(), extension->GetResourceURL("popup.html"));

  EXPECT_EQ("Goodbye", extension->page_action()->GetTitle(tab_id()));
  EXPECT_EQ(extension->GetResourceURL("popup.html"),
            extension->page_action()->GetPopupUrl(tab_id()));

  // Within-page navigation should keep the settings.
  NavigateAndCommit(GURL("http://www.google.com/#hash"));

  EXPECT_EQ("Goodbye", extension->page_action()->GetTitle(tab_id()));
  EXPECT_EQ(extension->GetResourceURL("popup.html"),
            extension->page_action()->GetPopupUrl(tab_id()));

  // Should discard the settings, and go back to the defaults.
  NavigateAndCommit(GURL("http://www.yahoo.com"));

  EXPECT_EQ("Hello", extension->page_action()->GetTitle(tab_id()));
  EXPECT_EQ(GURL(), extension->page_action()->GetPopupUrl(tab_id()));
};

}  // namespace
}  // namespace extensions
