// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_tasks.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "extensions/browser/entry_info.h"
#include "net/base/mime_util.h"

namespace file_manager {
namespace file_tasks {

class FileTasksBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    test::AddDefaultComponentExtensionsOnMainThread(browser()->profile());
  }
};

// Tests the default handlers for various file types in ChromeOS. This test
// exists to ensure the default app that launches when you open a file in the
// ChromeOS file manager does not change unexpectedly. Multiple default apps are
// allowed to register a handler for the same file type. Without that, it is not
// possible for an app to open that type even when given explicit direction via
// the chrome.fileManagerPrivate.executeTask API. The current conflict
// resolution mechanism is "sort by extension ID", which has the desired result.
// If desires change, we'll need to update ChooseAndSetDefaultTask() with some
// additional logic.
IN_PROC_BROWSER_TEST_F(FileTasksBrowserTest, DefaultHandlerChangeDetector) {
  constexpr struct {
    const char* file_extension;
    const char* app_id;
    bool has_mime = true;
  } kExpectations[] = {
      // Images.
      {"bmp", kGalleryAppId},
      {"gif", kGalleryAppId},
      {"ico", kGalleryAppId},
      {"jpg", kGalleryAppId},
      {"jpeg", kGalleryAppId},
      {"png", kGalleryAppId},
      {"webp", kGalleryAppId},

      // Raw.
      {"arw", kGalleryAppId, false},
      {"cr2", kGalleryAppId, false},
      {"dng", kGalleryAppId, false},
      {"nef", kGalleryAppId, false},
      {"nrw", kGalleryAppId, false},
      {"orf", kGalleryAppId, false},
      {"raf", kGalleryAppId, false},
      {"rw2", kGalleryAppId, false},

      // Video.
      {"3gp", kVideoPlayerAppId, false},
      {"avi", kVideoPlayerAppId, false},
      {"m4v", kVideoPlayerAppId},
      {"mkv", kVideoPlayerAppId, false},
      {"mov", kVideoPlayerAppId, false},
      {"mp4", kVideoPlayerAppId},
      {"mpeg", kVideoPlayerAppId},
      {"mpeg4", kVideoPlayerAppId, false},
      {"mpg", kVideoPlayerAppId},
      {"mpg4", kVideoPlayerAppId, false},
      {"ogm", kVideoPlayerAppId},
      {"ogv", kVideoPlayerAppId},
      {"ogx", kVideoPlayerAppId, false},
      {"webm", kVideoPlayerAppId},

      // Audio.
      {"amr", kAudioPlayerAppId, false},
      {"flac", kAudioPlayerAppId},
      {"m4a", kAudioPlayerAppId},
      {"mp3", kAudioPlayerAppId},
      {"oga", kAudioPlayerAppId},
      {"ogg", kAudioPlayerAppId},
      {"wav", kAudioPlayerAppId},
  };

  base::RunLoop run_loop;
  int remaining = base::size(kExpectations);
  auto callback = base::BindRepeating(
      [](base::RepeatingClosure quit, int* remaining,
         const char* file_extension, const char* expected_app_id,
         std::unique_ptr<std::vector<FullTaskDescriptor>> result) {
        ASSERT_TRUE(result) << file_extension;

        auto default_task =
            std::find_if(result->begin(), result->end(),
                         [](const auto& task) { return task.is_default(); });

        ASSERT_TRUE(default_task != result->end()) << file_extension;

        EXPECT_EQ(expected_app_id, default_task->task_descriptor().app_id)
            << " for extension: " << file_extension;

        // Verify no other task is set as default.
        EXPECT_EQ(1u, std::count_if(
                          result->begin(), result->end(),
                          [](const auto& task) { return task.is_default(); }))
            << file_extension;

        if (--*remaining == 0)
          quit.Run();
      },
      run_loop.QuitClosure(), &remaining);

  const base::FilePath prefix = base::FilePath().AppendASCII("file");

  for (const auto& test : kExpectations) {
    base::FilePath path = prefix.AddExtension(test.file_extension);

    // Fetching a mime type is part of the default app determination, but it
    // doesn't need to succeed.
    std::string mime_type;
    EXPECT_EQ(test.has_mime, net::GetMimeTypeFromFile(path, &mime_type))
        << test.file_extension;

    std::vector<extensions::EntryInfo> entries = {{path, mime_type, false}};
    std::vector<GURL> file_urls{GURL()};
    FindAllTypesOfTasks(
        browser()->profile(), entries, file_urls,
        base::BindRepeating(callback, test.file_extension, test.app_id));
  }
  run_loop.Run();
  EXPECT_EQ(0, remaining);
}

}  // namespace file_tasks
}  // namespace file_manager
