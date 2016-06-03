// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include <tuple>

#include "base/test/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/translate/content/common/translate_messages.h"
#include "components/translate/content/renderer/translate_helper.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebView.h"

// Constants for UMA statistic collection.
static const char kTranslateCaptureText[] = "Translate.CaptureText";

class ChromeRenderFrameObserverTest : public ChromeRenderViewTest {};

TEST_F(ChromeRenderFrameObserverTest, SkipCapturingSubFrames) {
  base::HistogramTester histogram_tester;
  LoadHTML(
      "<!DOCTYPE html><body>"
      "This is a main document"
      "<iframe srcdoc=\"This a document in an iframe.\">"
      "</body>");
  view_->GetWebView()->updateAllLifecyclePhases();
  const IPC::Message* message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeFrameHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeFrameHostMsg_TranslateLanguageDetermined::Param params;
  ChromeFrameHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_TRUE(std::get<1>(params)) << "Page should be translatable.";
  // Should have 2 samples: one for preliminary capture, one for final capture.
  // If there are more, then subframes are being captured more than once.
  histogram_tester.ExpectTotalCount(kTranslateCaptureText, 2);
}
