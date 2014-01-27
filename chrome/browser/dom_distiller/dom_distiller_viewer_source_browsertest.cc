// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/dom_distiller/content/dom_distiller_viewer_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

// WebContents observer that stores reference to the current |RenderViewHost|.
class LoadSuccessObserver : public content::WebContentsObserver {
 public:
  explicit LoadSuccessObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        validated_url_(GURL()),
        finished_load_(false),
        load_failed_(false),
        render_view_host_(NULL) {}

  virtual void DidFinishLoad(int64 frame_id,
                             const GURL& validated_url,
                             bool is_main_frame,
                             content::RenderViewHost* render_view_host)
      OVERRIDE {
    validated_url_ = validated_url;
    finished_load_ = true;
    render_view_host_ = render_view_host;
  }

  virtual void DidFailProvisionalLoad(int64 frame_id,
                                      const base::string16& frame_unique_name,
                                      bool is_main_frame,
                                      const GURL& validated_url,
                                      int error_code,
                                      const base::string16& error_description,
                                      content::RenderViewHost* render_view_host)
      OVERRIDE {
    load_failed_ = true;
  }

  const GURL& validated_url() const { return validated_url_; }
  bool finished_load() const { return finished_load_; }
  bool load_failed() const { return load_failed_; }

  const content::RenderViewHost* render_view_host() const {
    return render_view_host_;
  }

 private:
  GURL validated_url_;
  bool finished_load_;
  bool load_failed_;
  content::RenderViewHost* render_view_host_;

  DISALLOW_COPY_AND_ASSIGN(LoadSuccessObserver);
};

class DomDistillerViewerSourceBrowserTest : public InProcessBrowserTest {
 public:
  DomDistillerViewerSourceBrowserTest() {}
  virtual ~DomDistillerViewerSourceBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableDomDistiller);
  }
};

// The DomDistillerViewerSource renders untrusted content, so ensure no bindings
// are enabled.
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest, NoWebUIBindings) {
  // Ensure the source is registered.
  // TODO(nyquist): Remove when the source is always registered on startup.
  content::URLDataSource::Add(
      browser()->profile(),
      new DomDistillerViewerSource(chrome::kDomDistillerScheme));

  // Setup observer to inspect the RenderViewHost after committed navigation.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  LoadSuccessObserver observer(contents);

  // Navigate to a URL which the source should respond to.
  std::string url_without_scheme = "://distilled";
  GURL url(chrome::kDomDistillerScheme + url_without_scheme);
  ui_test_utils::NavigateToURL(browser(), url);

  // A navigation should have succeeded to the correct URL.
  ASSERT_FALSE(observer.load_failed());
  ASSERT_TRUE(observer.finished_load());
  ASSERT_EQ(url, observer.validated_url());
  // Ensure no bindings.
  const content::RenderViewHost* render_view_host = observer.render_view_host();
  ASSERT_EQ(0, render_view_host->GetEnabledBindings());
}

}  // namespace dom_distiller
