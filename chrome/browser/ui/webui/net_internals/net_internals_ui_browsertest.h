// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_BROWSERTEST_H_
#pragma once

#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "content/public/browser/web_ui_message_handler.h"

class GURL;

namespace base {
class ListValue;
}  // namespace base

class NetInternalsTest : public WebUIBrowserTest {
 protected:
  NetInternalsTest();
  virtual ~NetInternalsTest();

 private:
  // Class to handle messages from the renderer needed by certain tests.
  class MessageHandler : public content::WebUIMessageHandler {
   public:
    explicit MessageHandler(NetInternalsTest* net_internals_test);

   private:
    virtual void RegisterMessages() OVERRIDE;

    // Runs NetInternalsTest.callback with the given value.
    void RunJavascriptCallback(base::Value* value);

    // Takes a string and provides the corresponding URL from the test server,
    // which must already have been started.
    void GetTestServerURL(const base::ListValue* list_value);

    // Called on UI thread.  Adds an entry to the cache for the specified
    // hostname by posting a task to the IO thread.  Takes the host name,
    // ip address, net error code, and expiration time in days from now
    // as parameters.  If the error code indicates failure, the ip address
    // must be an empty string.
    void AddCacheEntry(const base::ListValue* list_value);

    // Opens a page in a new tab that prerenders the given URL.
    void PrerenderPage(const base::ListValue* list_value);

    // Navigates to the prerender in the background tab. This assumes that
    // there is a "Click()" function in the background tab which will navigate
    // there, and that the background tab exists at slot 1.
    void NavigateToPrerender(const base::ListValue* list_value);

    // Creates an incognito browser.  Once creation is complete, passes a
    // message to the Javascript test harness.
    void CreateIncognitoBrowser(const base::ListValue* list_value);

    // Closes an incognito browser created with CreateIncognitoBrowser.
    void CloseIncognitoBrowser(const base::ListValue* list_value);

    // Takes in a boolean and enables/disabled HTTP pipelining accordingly.
    void EnableHttpPipelining(const base::ListValue* list_value);

    // Called on UI thread. Adds an entry to the list of known HTTP pipelining
    // hosts.
    void AddDummyHttpPipelineFeedback(const base::ListValue* list_value);

    Browser* browser() {
      return net_internals_test_->browser();
    }

    NetInternalsTest* net_internals_test_;
    Browser* incognito_browser_;

    DISALLOW_COPY_AND_ASSIGN(MessageHandler);
  };

  // InProcessBrowserTest overrides.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

  // WebUIBrowserTest implementation.
  virtual content::WebUIMessageHandler* GetMockMessageHandler() OVERRIDE;

  GURL CreatePrerenderLoaderUrl(const GURL& prerender_url);

  // Attempts to start the test server.  Returns true on success or if the
  // TestServer is already started.
  bool StartTestServer();

  MessageHandler message_handler_;

  // True if the test server has already been successfully started.
  bool test_server_started_;

  DISALLOW_COPY_AND_ASSIGN(NetInternalsTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_BROWSERTEST_H_
