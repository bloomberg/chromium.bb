// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

namespace {
// Test for audible playback message.
class WaitForAudioContextAudible : WebContentsObserver {
 public:
  explicit WaitForAudioContextAudible(WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    run_loop_.Run();
  }

  void AudioContextPlaybackStarted(const AudioContextId&) final {
    // Stop the run loop when we get the message
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WaitForAudioContextAudible);
};

// Test for silent playback started (audible playback stopped).
class WaitForAudioContextSilent : WebContentsObserver {
 public:
  explicit WaitForAudioContextSilent(WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    run_loop_.Run();
  }

  void AudioContextPlaybackStopped(const AudioContextId&) final {
    // Stop the run loop when we get the message
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WaitForAudioContextSilent);
};

}  // namespace

class AudioContextManagerTest : public ContentBrowserTest {};

IN_PROC_BROWSER_TEST_F(AudioContextManagerTest, AudioContextPlaybackRecorded) {
  NavigateToURL(shell(),
                content::GetTestUrl("media/webaudio/", "playback-test.html"));

  // Set gain to 1 to start audible audio and verify we got the
  // playback started message.
  {
    ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "gain.gain.value = 1;"));
    WaitForAudioContextAudible wait(shell()->web_contents());
  }

  // Set gain to 0 to stop audible audio and verify we got the
  // playback stopped message.
  {
    ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "gain.gain.value = 0;"));
    WaitForAudioContextSilent wait(shell()->web_contents());
  }
}

}  // namespace content
