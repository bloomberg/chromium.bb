// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/log_web_ui_url.h"

#include <vector>

#include "base/hash.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using base::Bucket;
using testing::ElementsAre;

namespace webui {

class LogWebUIUrlTest : public InProcessBrowserTest {
 public:
  LogWebUIUrlTest() {}
  ~LogWebUIUrlTest() override {}

  std::vector<Bucket> GetSamples() {
    return histogram_tester_.GetAllSamples(webui::kWebUICreatedForUrl);
  }

 private:
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(LogWebUIUrlTest);
};

IN_PROC_BROWSER_TEST_F(LogWebUIUrlTest, TestHistoryFrame) {
  GURL history_frame_url(chrome::kChromeUIHistoryFrameURL);

  ui_test_utils::NavigateToURL(browser(), history_frame_url);

  uint32 history_frame_url_hash = base::Hash(history_frame_url.spec());
  EXPECT_THAT(GetSamples(), ElementsAre(Bucket(history_frame_url_hash, 1)));

  chrome::Reload(browser(), CURRENT_TAB);

  EXPECT_THAT(GetSamples(), ElementsAre(Bucket(history_frame_url_hash, 2)));
}

// TODO(dbeam): test an uber page URL like chrome://history.

}  // namespace webui
