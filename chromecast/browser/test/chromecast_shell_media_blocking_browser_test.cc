// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/browser/test/chromecast_browser_test.h"
#include "chromecast/browser/test/chromecast_browser_test_helper.h"
#include "chromecast/chromecast_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/test_data_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace chromecast {
namespace shell {

class ChromecastShellMediaBlockingBrowserTest : public ChromecastBrowserTest {
 public:
  ChromecastShellMediaBlockingBrowserTest() {}

 protected:
  void PlayMedia(const std::string& tag, const std::string& media_file) {
    base::StringPairs query_params;
    query_params.push_back(std::make_pair(tag, media_file));
    query_params.push_back(std::make_pair("loop", "true"));

    std::string query = media::GetURLQueryString(query_params);
    GURL gurl = content::GetFileUrlWithQuery(
        media::GetTestDataFilePath("player.html"), query);

    web_contents_ = helper_->NavigateToURL(gurl);
    WaitForLoadStop(web_contents_);
  }

  void BlockAndTestPlayerState(const std::string& media_type, bool blocked) {
    helper_->BlockMediaLoading(blocked);

    // Changing states is not instant, but should be timely (< 0.5s).
    for (size_t i = 0; i < 5; i++) {
      base::RunLoop run_loop;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(),
          base::TimeDelta::FromMilliseconds(50));
      base::MessageLoop::ScopedNestableTaskAllower allow_nested(
          base::MessageLoop::current());
      run_loop.Run();

      const std::string command =
          "document.getElementsByTagName(\"" + media_type + "\")[0].paused";
      const std::string js =
          "window.domAutomationController.send(" + command + ");";

      bool paused;
      ASSERT_TRUE(ExecuteScriptAndExtractBool(web_contents_, js, &paused));

      if (paused == blocked) {
        SUCCEED() << "Media element has been successfullly "
                  << (blocked ? "blocked" : "unblocked");
        return;
      }
    }

    FAIL() << "Could not successfullly " << (blocked ? "block" : "unblock")
           << " media element";
  }

  content::WebContents* web_contents_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromecastShellMediaBlockingBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ChromecastShellMediaBlockingBrowserTest,
                       Audio_BlockUnblock) {
  PlayMedia("audio", "bear-audio-10s-CBR-has-TOC.mp3");

  BlockAndTestPlayerState("audio", true);
  BlockAndTestPlayerState("audio", false);
}

#if !BUILDFLAG(IS_CAST_AUDIO_ONLY)
IN_PROC_BROWSER_TEST_F(ChromecastShellMediaBlockingBrowserTest,
                       Video_BlockUnblock) {
  PlayMedia("video", "tulip2.webm");

  BlockAndTestPlayerState("video", true);
  BlockAndTestPlayerState("video", false);
}
#endif

}  // namespace shell
}  // namespace chromecast
