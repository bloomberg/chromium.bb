// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MediaGalleriesPrivate gallery watch API browser tests.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/render_view_host.h"

namespace {

// Id of test extension from
// chrome/test/data/extensions/api_test/|kTestExtensionPath|
const char kTestExtensionId[] = "gceegfkgibmgpfopknlcgleimclbknie";
const char kTestExtensionPath[] = "media_galleries_private/gallerywatch";

// JS commands.
const char kAddGalleryChangedListenerCmd[] = "addGalleryChangedListener()";
const char kGetAllWatchedGalleryIdsCmd[] = "getAllWatchedGalleryIds()";
const char kGetMediaFileSystemsCmd[] = "getMediaFileSystems()";
const char kRemoveAllGalleryWatchCmd[] = "removeAllGalleryWatch()";
const char kRemoveGalleryChangedListenerCmd[] =
    "removeGalleryChangedListener()";
const char kRemoveGalleryWatchCmd[] = "removeGalleryWatch()";
const char kSetupWatchOnValidGalleriesCmd[] = "setupWatchOnValidGalleries()";
const char kSetupWatchOnInvalidGalleryCmd[] = "setupWatchOnInvalidGallery()";

// And JS reply messages.
const char kAddGalleryWatchOK[] = "add_gallery_watch_ok";
const char kAddGalleryChangedListenerOK[] = "add_gallery_changed_listener_ok";
const char kGetAllGalleryWatchOK[] = "get_all_gallery_watch_ok";
const char kGetMediaFileSystemsOK[] = "get_media_file_systems_ok";
const char kGetMediaFileSystemsCallbackOK[] =
    "get_media_file_systems_callback_ok";
const char kRemoveAllGalleryWatchOK[] = "remove_all_gallery_watch_ok";
const char kRemoveGalleryChangedListenerOK[] =
    "remove_gallery_changed_listener_ok";
const char kRemoveGalleryWatchOK[] = "remove_gallery_watch_ok";

// Test reply messages.
const char kAddGalleryWatchRequestSucceeded[] = "add_watch_request_succeeded";
const char kAddGalleryWatchRequestFailed[] = "add_watch_request_failed";
const char kGalleryChangedEventReceived[] = "gallery_changed_event_received";
const char kGetAllGalleryWatchResultA[] = "gallery_watchers_does_not_exists";
const char kGetAllGalleryWatchResultB[] =
    "watchers_for_galleries_{1, 2, 3}_found";

}  // namespace


///////////////////////////////////////////////////////////////////////////////
//                 MediaGalleriesPrivateGalleryWatchApiTest                  //
///////////////////////////////////////////////////////////////////////////////

class MediaGalleriesPrivateGalleryWatchApiTest : public ExtensionApiTest {
 public:
  MediaGalleriesPrivateGalleryWatchApiTest() {}
  virtual ~MediaGalleriesPrivateGalleryWatchApiTest() {}

 protected:
  // ExtensionApiTest overrides.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kWhitelistedExtensionID,
                                    kTestExtensionId);
  }

  void ExecuteCmdAndCheckReply(content::RenderViewHost* host,
                              const std::string& js_command,
                              const std::string& ok_message) {
    ExtensionTestMessageListener listener(ok_message, false);
    host->ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16(js_command));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  bool AddNewFileInGallery(int gallery_directory_key) {
    if ((gallery_directory_key != chrome::DIR_USER_MUSIC) &&
        (gallery_directory_key != chrome::DIR_USER_PICTURES) &&
        (gallery_directory_key != chrome::DIR_USER_VIDEOS))
      return false;

    base::FilePath gallery_dir;
    if (!PathService::Get(gallery_directory_key, &gallery_dir))
      return false;
    base::FilePath gallery_file =
        gallery_dir.Append(FILE_PATH_LITERAL("test1.txt"));
    std::string content("new content");
    int write_size = file_util::WriteFile(gallery_file, content.c_str(),
                                          content.length());
    return (write_size == static_cast<int>(content.length()));
  }

  // Loads the test extension and returns the RenderViewHost of the extension.
  // Returns NULL if the extension load operation failed.
  content::RenderViewHost* GetBackgroundHostForTestExtension() {
    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII(kTestExtensionPath));
    if (!extension)
      return NULL;
    return extensions::ExtensionSystem::Get(browser()->profile())->
        process_manager()->GetBackgroundHostForExtension(extension->id())->
           render_view_host();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPrivateGalleryWatchApiTest);
};


///////////////////////////////////////////////////////////////////////////////
//                               TESTS                                       //
///////////////////////////////////////////////////////////////////////////////
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       BasicGalleryWatch) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  content::RenderViewHost* host = GetBackgroundHostForTestExtension();
  ASSERT_TRUE(host);

  // Get media file systems.
  ExtensionTestMessageListener get_media_systems_finished(
      kGetMediaFileSystemsCallbackOK, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetMediaFileSystemsCmd,
                          kGetMediaFileSystemsOK);
  EXPECT_TRUE(get_media_systems_finished.WaitUntilSatisfied());

  // Set up gallery watch.
  ExtensionTestMessageListener add_gallery_watch_finished(
      kAddGalleryWatchRequestSucceeded, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kSetupWatchOnValidGalleriesCmd,
                          kAddGalleryWatchOK);
  EXPECT_TRUE(add_gallery_watch_finished.WaitUntilSatisfied());

  // Add gallery watch listener.
  ExecuteCmdAndCheckReply(host, kAddGalleryChangedListenerCmd,
                          kAddGalleryChangedListenerOK);

  // Modify gallery contents.
  ExtensionTestMessageListener music_gallery_change_event_received(
      kGalleryChangedEventReceived, false  /* no reply */);
  ASSERT_TRUE(AddNewFileInGallery(chrome::DIR_USER_MUSIC));
  EXPECT_TRUE(music_gallery_change_event_received.WaitUntilSatisfied());

  ExtensionTestMessageListener pictures_gallery_change_event_received(
      kGalleryChangedEventReceived, false  /* no reply */);
  ASSERT_TRUE(AddNewFileInGallery(chrome::DIR_USER_PICTURES));
  EXPECT_TRUE(pictures_gallery_change_event_received.WaitUntilSatisfied());

  ExtensionTestMessageListener videos_gallery_change_event_received(
      kGalleryChangedEventReceived, false  /* no reply */);
  ASSERT_TRUE(AddNewFileInGallery(chrome::DIR_USER_VIDEOS));
  EXPECT_TRUE(videos_gallery_change_event_received.WaitUntilSatisfied());

  // Remove gallery watch listener.
  ExecuteCmdAndCheckReply(host, kRemoveGalleryChangedListenerCmd,
                          kRemoveGalleryChangedListenerOK);

  // Remove gallery watch request.
  ExecuteCmdAndCheckReply(host, kRemoveGalleryWatchCmd, kRemoveGalleryWatchOK);
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       RemoveListenerAndModifyGallery) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  content::RenderViewHost* host = GetBackgroundHostForTestExtension();
  ASSERT_TRUE(host);

  // Get media file systems.
  ExtensionTestMessageListener get_media_systems_finished(
      kGetMediaFileSystemsCallbackOK, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetMediaFileSystemsCmd,
                          kGetMediaFileSystemsOK);
  EXPECT_TRUE(get_media_systems_finished.WaitUntilSatisfied());

  // Set up gallery watch.
  ExtensionTestMessageListener add_gallery_watch_finished(
      kAddGalleryWatchRequestSucceeded, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kSetupWatchOnValidGalleriesCmd,
                         kAddGalleryWatchOK);
  EXPECT_TRUE(add_gallery_watch_finished.WaitUntilSatisfied());

  // Add a gallery watch listener.
  ExecuteCmdAndCheckReply(host, kAddGalleryChangedListenerCmd,
                          kAddGalleryChangedListenerOK);
  // Modify gallery contents.
  ExtensionTestMessageListener music_gallery_change_event_received(
      kGalleryChangedEventReceived, false  /* no reply */);
  ASSERT_TRUE(AddNewFileInGallery(chrome::DIR_USER_MUSIC));
  EXPECT_TRUE(music_gallery_change_event_received.WaitUntilSatisfied());

  // Remove gallery watch listener.
  ExecuteCmdAndCheckReply(host, kRemoveGalleryChangedListenerCmd,
                          kRemoveGalleryChangedListenerOK);

  // No listener, modify gallery contents.
  ASSERT_TRUE(AddNewFileInGallery(chrome::DIR_USER_MUSIC));

  // Remove gallery watch.
  ExecuteCmdAndCheckReply(host, kRemoveGalleryWatchCmd, kRemoveGalleryWatchOK);
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       SetupGalleryWatchWithoutListeners) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  content::RenderViewHost* host = GetBackgroundHostForTestExtension();
  ASSERT_TRUE(host);

  // Get media file systems.
  ExtensionTestMessageListener get_media_systems_finished(
      kGetMediaFileSystemsCallbackOK, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetMediaFileSystemsCmd,
                          kGetMediaFileSystemsOK);
  EXPECT_TRUE(get_media_systems_finished.WaitUntilSatisfied());

  // Set up gallery watch.
  ExecuteCmdAndCheckReply(host, kSetupWatchOnValidGalleriesCmd,
                          kAddGalleryWatchOK);

  // No listeners, modify gallery contents.
  ExtensionTestMessageListener music_gallery_change_event_received(
      kGalleryChangedEventReceived, false  /* no reply */);
  ASSERT_TRUE(AddNewFileInGallery(chrome::DIR_USER_MUSIC));

  // Remove gallery watch.
  ExecuteCmdAndCheckReply(host, kRemoveGalleryWatchCmd, kRemoveGalleryWatchOK);
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       SetupGalleryChangedListenerWithoutWatchers) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  content::RenderViewHost* host = GetBackgroundHostForTestExtension();
  ASSERT_TRUE(host);

  // Get media file systems.
  ExtensionTestMessageListener get_media_systems_finished(
      kGetMediaFileSystemsCallbackOK, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetMediaFileSystemsCmd,
                          kGetMediaFileSystemsOK);
  EXPECT_TRUE(get_media_systems_finished.WaitUntilSatisfied());

  // Add gallery watch listener.
  ExecuteCmdAndCheckReply(host, kAddGalleryChangedListenerCmd,
                          kAddGalleryChangedListenerOK);

  // Modify gallery contents. Listener should not get called because add watch
  // request was not called.
  ExtensionTestMessageListener music_gallery_change_event_received(
      kGalleryChangedEventReceived, false  /* no reply */);
  ASSERT_TRUE(AddNewFileInGallery(chrome::DIR_USER_MUSIC));

  // Remove gallery watch listener.
  ExecuteCmdAndCheckReply(host, kRemoveGalleryChangedListenerCmd,
                          kRemoveGalleryChangedListenerOK);
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       SetupWatchOnInvalidGallery) {
  content::RenderViewHost* host = GetBackgroundHostForTestExtension();
  ASSERT_TRUE(host);

  // Set up a invalid gallery watch.
  ExtensionTestMessageListener invalid_gallery_watch_request_finished(
      kAddGalleryWatchRequestFailed, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kSetupWatchOnInvalidGalleryCmd,
                          kAddGalleryWatchOK);
  EXPECT_TRUE(invalid_gallery_watch_request_finished.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       GetAllGalleryWatch) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  content::RenderViewHost* host = GetBackgroundHostForTestExtension();
  ASSERT_TRUE(host);

  // Get media file systems.
  ExtensionTestMessageListener get_media_systems_finished(
      kGetMediaFileSystemsCallbackOK, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetMediaFileSystemsCmd,
                          kGetMediaFileSystemsOK);
  EXPECT_TRUE(get_media_systems_finished.WaitUntilSatisfied());

  // Gallery watchers are not yet added.
  // chrome.mediaGalleriesPrivate.getAllGalleryWatch should return an empty
  // list.
  ExtensionTestMessageListener initial_get_all_check_finished(
      kGetAllGalleryWatchResultA, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetAllWatchedGalleryIdsCmd,
                          kGetAllGalleryWatchOK);
  EXPECT_TRUE(initial_get_all_check_finished.WaitUntilSatisfied());

  // Set up gallery watchers.
  ExtensionTestMessageListener add_gallery_watch_finished(
      kAddGalleryWatchRequestSucceeded, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kSetupWatchOnValidGalleriesCmd,
                          kAddGalleryWatchOK);
  EXPECT_TRUE(add_gallery_watch_finished.WaitUntilSatisfied());

  // chrome.mediaGalleriesPrivate.getAllGalleryWatch should return the
  // gallery identifiers.
  ExtensionTestMessageListener get_all_watched_galleries_finished(
      kGetAllGalleryWatchResultB, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetAllWatchedGalleryIdsCmd,
                          kGetAllGalleryWatchOK);
  EXPECT_TRUE(get_all_watched_galleries_finished.WaitUntilSatisfied());

  // Remove gallery watch request.
  ExecuteCmdAndCheckReply(host, kRemoveGalleryWatchCmd, kRemoveGalleryWatchOK);

  // Gallery watchers removed.
  // chrome.mediaGalleriesPrivate.getAllGalleryWatch() should return an empty
  // list.
  ExtensionTestMessageListener final_get_all_check_finished(
      kGetAllGalleryWatchResultA, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetAllWatchedGalleryIdsCmd,
                          kGetAllGalleryWatchOK);
  EXPECT_TRUE(final_get_all_check_finished.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       RemoveAllGalleryWatch) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  content::RenderViewHost* host = GetBackgroundHostForTestExtension();
  ASSERT_TRUE(host);

  // Get media file systems.
  ExtensionTestMessageListener get_media_systems_finished(
      kGetMediaFileSystemsCallbackOK, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetMediaFileSystemsCmd,
                          kGetMediaFileSystemsOK);
  EXPECT_TRUE(get_media_systems_finished.WaitUntilSatisfied());

  // Set up gallery watchers.
  ExtensionTestMessageListener add_gallery_watch_finished(
      kAddGalleryWatchRequestSucceeded, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kSetupWatchOnValidGalleriesCmd,
                          kAddGalleryWatchOK);
  EXPECT_TRUE(add_gallery_watch_finished.WaitUntilSatisfied());

  // chrome.mediaGalleriesPrivate.getAllGalleryWatch should return the watched
  // gallery identifiers.
  ExtensionTestMessageListener get_all_watched_galleries_finished(
      kGetAllGalleryWatchResultB, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetAllWatchedGalleryIdsCmd,
                          kGetAllGalleryWatchOK);
  EXPECT_TRUE(get_all_watched_galleries_finished.WaitUntilSatisfied());

  // Remove all gallery watchers.
  ExecuteCmdAndCheckReply(host, kRemoveAllGalleryWatchCmd,
                          kRemoveAllGalleryWatchOK);

  // Gallery watchers removed. chrome.mediaGalleriesPrivate.getAllGalleryWatch
  // should return an empty list.
  ExtensionTestMessageListener final_get_all_check_finished(
      kGetAllGalleryWatchResultA, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetAllWatchedGalleryIdsCmd,
                          kGetAllGalleryWatchOK);
  EXPECT_TRUE(final_get_all_check_finished.WaitUntilSatisfied());
}
#endif

#if !defined(OS_WIN) && !defined(OS_CHROMEOS)
// Gallery watch request is not enabled on non-windows platforms.
// Please refer to crbug.com/144491.
IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       SetupGalleryWatch) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  content::RenderViewHost* host = GetBackgroundHostForTestExtension();
  ASSERT_TRUE(host);

  // Get media file systems.
  ExtensionTestMessageListener get_media_systems_finished(
      kGetMediaFileSystemsCallbackOK, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetMediaFileSystemsCmd,
                          kGetMediaFileSystemsOK);
  EXPECT_TRUE(get_media_systems_finished.WaitUntilSatisfied());

  // Set up a invalid gallery watch.
  ExtensionTestMessageListener gallery_watch_request_finished(
      kAddGalleryWatchRequestFailed, false  /* no reply */);
  // Set up gallery watch.
  ExecuteCmdAndCheckReply(host, kSetupWatchOnValidGalleriesCmd,
                          kAddGalleryWatchOK);
  EXPECT_TRUE(gallery_watch_request_finished.WaitUntilSatisfied());
}

// Gallery watch request is not enabled on non-windows platforms.
// Please refer to crbug.com/144491.
IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       GetAllGalleryWatch) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  content::RenderViewHost* host = GetBackgroundHostForTestExtension();
  ASSERT_TRUE(host);

  // Get media file systems.
  ExtensionTestMessageListener get_media_systems_finished(
      kGetMediaFileSystemsCallbackOK, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetMediaFileSystemsCmd,
                          kGetMediaFileSystemsOK);
  EXPECT_TRUE(get_media_systems_finished.WaitUntilSatisfied());

  // Set up gallery watch.
  ExecuteCmdAndCheckReply(host, kSetupWatchOnValidGalleriesCmd,
                          kAddGalleryWatchOK);

  // Gallery watchers does not exists.
  // chrome.mediaGalleriesPrivate.getAllGalleryWatch should return an empty
  // list.
  ExtensionTestMessageListener get_all_gallery_watch_finished(
      kGetAllGalleryWatchResultA, false  /* no reply */);
  ExecuteCmdAndCheckReply(host, kGetAllWatchedGalleryIdsCmd,
                          kGetAllGalleryWatchOK);
  EXPECT_TRUE(get_all_gallery_watch_finished.WaitUntilSatisfied());
}
#endif
