// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_media_session_manager.h"

#include <iostream>
#include <sstream>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/media/android/media_web_contents_observer_android.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Ne;

namespace content {

namespace {

// Helper function for build test javascripts.
std::string BuildSetMetadataScript(const MediaMetadata& metadata) {
  std::ostringstream generated_script;

  generated_script << "var audio = document.createElement(\'audio\');"
     << "audio.session = new MediaSession();"
     << "audio.session.metadata = new MediaMetadata({"
     << "title: \"" << metadata.title << "\", "
     << "artist: \"" << metadata.artist << "\", "
     << "album: \"" << metadata.album << "\", "
     << "artwork: [";

  std::string artwork_separator = "";
  for (const auto& artwork : metadata.artwork) {
    generated_script << artwork_separator << "{"
       << "src: \"" << artwork.src.spec() << "\", "
       << "type: \"" << artwork.type.string() << "\", "
       << "sizes: \"";
    for (const auto& size : artwork.sizes) {
      generated_script << size.width() << "x" << size.height() << " ";
    }
    generated_script << "\"}";
    artwork_separator = ", ";
  }
  generated_script << "]});";

  return generated_script.str();
}

}  // anonymous namespace

// Helper function to be pretty-print error messages by GMock.
void PrintTo(const MediaMetadata& metadata, std::ostream* os) {
  *os << "{ title=" << metadata.title << ", ";
  *os << "artist=" << metadata.artist << ", ";
  *os << "album=" << metadata.album << ", ";
  *os << "artwork=[";
  for (const auto& artwork : metadata.artwork) {
    *os << "{ src=" << artwork.src.spec() << ", ";
    *os << "type=" << artwork.type.string() << ", ";
    *os << "sizes=[";
    for (const auto& size : artwork.sizes) {
      *os <<  size.width() << "x" << size.height() << " ";
    }
    *os << "]}";
  }
  *os << "]}";
}

class MockBrowserMediaSessionManager : public BrowserMediaSessionManager {
 public:
  explicit MockBrowserMediaSessionManager(RenderFrameHost* render_frame_host)
      : BrowserMediaSessionManager(render_frame_host) {}

  MOCK_METHOD2(OnActiveate, void(int session_id, int request_id));
  MOCK_METHOD2(OnDeactiveate, void(int session_id, int request_id));
  MOCK_METHOD2(OnSetMetadata, void(int session_id,
                                   const MediaMetadata& metadata));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBrowserMediaSessionManager);
};

class BrowserMediaSessionManagerBrowserTest : public ContentBrowserTest {
 public:
  BrowserMediaSessionManagerBrowserTest() = default;
  ~BrowserMediaSessionManagerBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    web_contents_ = shell()->web_contents();

    std::unique_ptr<MockBrowserMediaSessionManager> manager(
        new MockBrowserMediaSessionManager(web_contents_->GetMainFrame()));
    browser_media_session_manager_ = manager.get();
    MediaWebContentsObserverAndroid::FromWebContents(web_contents_)
        ->SetMediaSessionManagerForTest(
            web_contents_->GetMainFrame(), std::move(manager));

    shell()->LoadURL(GURL("about:blank"));

    ON_CALL(*browser_media_session_manager_, OnSetMetadata(_, _))
        .WillByDefault(InvokeWithoutArgs([&]{
              message_loop_runner_->Quit();
            }));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "MediaSession");
  }

  WebContents* web_contents_;
  MockBrowserMediaSessionManager* browser_media_session_manager_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

IN_PROC_BROWSER_TEST_F(BrowserMediaSessionManagerBrowserTest,
                       TestMetadataPropagated) {
  MediaMetadata expected;
  expected.title = base::ASCIIToUTF16("title1");
  expected.artist = base::ASCIIToUTF16("artist1");
  expected.album = base::ASCIIToUTF16("album1");
  MediaMetadata::Artwork artwork;
  artwork.src = GURL("http://foo.com/bar.png");
  artwork.type = base::NullableString16(base::ASCIIToUTF16("image/png"), false);
  artwork.sizes.push_back(gfx::Size(128, 128));
  expected.artwork.push_back(artwork);

  message_loop_runner_ = new MessageLoopRunner();
  EXPECT_CALL(*browser_media_session_manager_, OnSetMetadata(_, expected))
      .Times(1);
  ASSERT_TRUE(ExecuteScript(web_contents_->GetMainFrame(),
                            BuildSetMetadataScript(expected)));
  message_loop_runner_->Run();
}

IN_PROC_BROWSER_TEST_F(BrowserMediaSessionManagerBrowserTest,
                       TestSetMetadataTwice) {
  // Make expectations ordered.
  InSequence s;

  MediaMetadata dont_care_metadata;

  MediaMetadata expected;
  expected.title = base::ASCIIToUTF16("title2");
  expected.artist = base::ASCIIToUTF16("artist2");
  expected.album = base::ASCIIToUTF16("album2");
  MediaMetadata::Artwork artwork;
  artwork.src = GURL("http://foo.com/bar.jpg");
  artwork.type = base::NullableString16(
      base::ASCIIToUTF16("image/jpeg"), false);
  artwork.sizes.push_back(gfx::Size(256, 256));
  expected.artwork.push_back(artwork);

  // Set metadata for the first time.
  message_loop_runner_ = new MessageLoopRunner();
  EXPECT_CALL(*browser_media_session_manager_,
              OnSetMetadata(_, dont_care_metadata))
      .Times(1);
  ASSERT_TRUE(ExecuteScript(web_contents_->GetMainFrame(),
                            BuildSetMetadataScript(dont_care_metadata)));
  message_loop_runner_->Run();

  // Set metadata for the second time.
  message_loop_runner_ = new MessageLoopRunner();
  EXPECT_CALL(*browser_media_session_manager_, OnSetMetadata(_, expected))
      .Times(1);
  ASSERT_TRUE(ExecuteScript(web_contents_->GetMainFrame(),
                            BuildSetMetadataScript(expected)));
  message_loop_runner_->Run();
}

IN_PROC_BROWSER_TEST_F(BrowserMediaSessionManagerBrowserTest,
                       TestFileArtworkRemoved) {
  // Make expectations ordered.
  InSequence s;

  MediaMetadata dirty_metadata;
  MediaMetadata::Artwork file_artwork;
  file_artwork.src = GURL("file:///foo/bar.jpg");
  file_artwork.type = base::NullableString16(
      base::ASCIIToUTF16("image/jpeg"), false);
  dirty_metadata.artwork.push_back(file_artwork);

  MediaMetadata expected;

  // Set metadata for the first time.
  message_loop_runner_ = new MessageLoopRunner();
  EXPECT_CALL(*browser_media_session_manager_, OnSetMetadata(_, expected))
      .Times(1);
  ASSERT_TRUE(ExecuteScript(web_contents_->GetMainFrame(),
                            BuildSetMetadataScript(dirty_metadata)));
  message_loop_runner_->Run();
}

}  // namespace content
