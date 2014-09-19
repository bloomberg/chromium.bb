// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MediaGalleriesPrivate gallery watch API browser tests.

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

// Id of test extension from
// chrome/test/data/extensions/api_test/|kTestExtensionPath|
const char kTestExtensionId[] = "gceegfkgibmgpfopknlcgleimclbknie";
const char kTestExtensionPath[] = "media_galleries_private/gallerywatch";

// JS commands.
const char kGetAllWatchedGalleryIdsCmd[] = "getAllWatchedGalleryIds()";
const char kGetMediaFileSystemsCmd[] = "getMediaFileSystems()";
const char kSetupWatchOnValidGalleriesCmd[] = "setupWatchOnValidGalleries()";
const char kAddGalleryChangedListenerCmd[] = "addGalleryChangedListener()";
const char kRemoveAllGalleryWatchCmd[] = "removeAllGalleryWatch()";
const char kRemoveGalleryChangedListenerCmd[] =
    "removeGalleryChangedListener()";
const char kRemoveGalleryWatchCmd[] = "removeGalleryWatch()";
const char kSetupWatchOnInvalidGalleryCmd[] = "setupWatchOnInvalidGallery()";

// And JS reply messages.
const char kAddGalleryWatchOK[] = "add_gallery_watch_ok";
const char kGetAllGalleryWatchOK[] = "get_all_gallery_watch_ok";
const char kGetMediaFileSystemsCallbackOK[] =
    "get_media_file_systems_callback_ok";
const char kGetMediaFileSystemsOK[] = "get_media_file_systems_ok";
const char kAddGalleryChangedListenerOK[] = "add_gallery_changed_listener_ok";
const char kRemoveAllGalleryWatchOK[] = "remove_all_gallery_watch_ok";
const char kRemoveGalleryChangedListenerOK[] =
    "remove_gallery_changed_listener_ok";
const char kRemoveGalleryWatchOK[] = "remove_gallery_watch_ok";

// Test reply messages.
const char kNoGalleryWatchesInstalled[] = "gallery_watchers_does_not_exists";
const char kAddGalleryWatchRequestFailed[] = "add_watch_request_failed";
const char kAddGalleryWatchRequestSucceeded[] = "add_watch_request_succeeded";
const char kGalleryChangedEventReceived[] = "gallery_changed_event_received";
const char kGalleryWatchesCheck[] = "gallery_watcher_checks";

}  // namespace


///////////////////////////////////////////////////////////////////////////////
//                 MediaGalleriesPrivateGalleryWatchApiTest                  //
///////////////////////////////////////////////////////////////////////////////

class MediaGalleriesPrivateGalleryWatchApiTest : public ExtensionApiTest {
 public:
  MediaGalleriesPrivateGalleryWatchApiTest()
      : extension_(NULL),
        background_host_(NULL) {
  }
  virtual ~MediaGalleriesPrivateGalleryWatchApiTest() {}

 protected:
  // ExtensionApiTest overrides.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        kTestExtensionId);
  }
  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    ensure_media_directories_exists_.reset(new EnsureMediaDirectoriesExists);
    extension_ = LoadExtension(test_data_dir_.AppendASCII(kTestExtensionPath));
    GetBackgroundHostForTestExtension();
    CreateTestGallery();
    FetchMediaGalleriesList();
  }
  virtual void TearDownOnMainThread() OVERRIDE {
    extension_ = NULL;
    background_host_ = NULL;
    ensure_media_directories_exists_.reset();
    ExtensionApiTest::TearDownOnMainThread();
  }

  bool GalleryWatchesSupported() {
    return base::FilePathWatcher::RecursiveWatchAvailable();
  }

  void ExecuteCmdAndCheckReply(const std::string& js_command,
                               const std::string& ok_message) {
    ExtensionTestMessageListener listener(ok_message, false);
    background_host_->GetMainFrame()->ExecuteJavaScript(
        base::ASCIIToUTF16(js_command));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  bool AddNewFileInTestGallery() {
    base::FilePath gallery_file =
        test_gallery_.path().Append(FILE_PATH_LITERAL("test1.txt"));
    std::string content("new content");
    int write_size = base::WriteFile(gallery_file, content.c_str(),
                                     content.length());
    return (write_size == static_cast<int>(content.length()));
  }

  void SetupGalleryWatches() {
    std::string expected_result = GalleryWatchesSupported() ?
        kAddGalleryWatchRequestSucceeded : kAddGalleryWatchRequestFailed;

    ExtensionTestMessageListener add_gallery_watch_finished(
        expected_result, false  /* no reply */);
    ExecuteCmdAndCheckReply(kSetupWatchOnValidGalleriesCmd, kAddGalleryWatchOK);
    EXPECT_TRUE(add_gallery_watch_finished.WaitUntilSatisfied());
  }

 private:
  void GetBackgroundHostForTestExtension() {
    ASSERT_TRUE(extension_);
    extensions::ExtensionSystem* extension_system =
        extensions::ExtensionSystem::Get(browser()->profile());
    background_host_ =
        extension_system->process_manager()->GetBackgroundHostForExtension(
            extension_->id())->render_view_host();
    ASSERT_TRUE(background_host_);
  }

  void CreateTestGallery() {
    MediaGalleriesPreferences* preferences =
        g_browser_process->media_file_system_registry()->GetPreferences(
            browser()->profile());
    base::RunLoop runloop;
    preferences->EnsureInitialized(runloop.QuitClosure());
    runloop.Run();

    ASSERT_TRUE(test_gallery_.CreateUniqueTempDir());
    MediaGalleryPrefInfo gallery_info;
    ASSERT_FALSE(preferences->LookUpGalleryByPath(test_gallery_.path(),
                                                  &gallery_info));
    MediaGalleryPrefId id = preferences->AddGallery(
        gallery_info.device_id,
        gallery_info.path,
        MediaGalleryPrefInfo::kAutoDetected,
        gallery_info.volume_label,
        gallery_info.vendor_name,
        gallery_info.model_name,
        gallery_info.total_size_in_bytes,
        gallery_info.last_attach_time,
        0, 0, 0);

    preferences->SetGalleryPermissionForExtension(*extension_, id, true);
  }

  void FetchMediaGalleriesList() {
    ExtensionTestMessageListener get_media_systems_finished(
        kGetMediaFileSystemsCallbackOK, false  /* no reply */);
    ExecuteCmdAndCheckReply(kGetMediaFileSystemsCmd, kGetMediaFileSystemsOK);
    EXPECT_TRUE(get_media_systems_finished.WaitUntilSatisfied());
  }

  scoped_ptr<EnsureMediaDirectoriesExists> ensure_media_directories_exists_;

  base::ScopedTempDir test_gallery_;

  const extensions::Extension* extension_;

  content::RenderViewHost* background_host_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPrivateGalleryWatchApiTest);
};

// Crashing on OSX.
#if defined(OS_MACOSX)
#define MAYBE_BasicGalleryWatch DISABLED_BasicGalleryWatch
#else
#define MAYBE_BasicGalleryWatch BasicGalleryWatch
#endif
IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       MAYBE_BasicGalleryWatch) {
  SetupGalleryWatches();

  // Add gallery watch listener.
  ExecuteCmdAndCheckReply(kAddGalleryChangedListenerCmd,
                          kAddGalleryChangedListenerOK);

  // Modify gallery contents.
  ExtensionTestMessageListener gallery_change_event_received(
      kGalleryChangedEventReceived, false  /* no reply */);
  ASSERT_TRUE(AddNewFileInTestGallery());
  if (GalleryWatchesSupported())
    EXPECT_TRUE(gallery_change_event_received.WaitUntilSatisfied());

  // Remove gallery watch listener.
  ExecuteCmdAndCheckReply(kRemoveGalleryChangedListenerCmd,
                          kRemoveGalleryChangedListenerOK);

  // Remove gallery watch request.
  if (GalleryWatchesSupported())
    ExecuteCmdAndCheckReply(kRemoveGalleryWatchCmd, kRemoveGalleryWatchOK);
}

// http://crbug.com/390979
IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       DISABLED_RemoveListenerAndModifyGallery) {
  if (!GalleryWatchesSupported())
    return;

  SetupGalleryWatches();

  // Add a gallery watch listener.
  ExecuteCmdAndCheckReply(kAddGalleryChangedListenerCmd,
                          kAddGalleryChangedListenerOK);
  // Modify gallery contents.
  ExtensionTestMessageListener gallery_change_event_received(
      kGalleryChangedEventReceived, false  /* no reply */);
  ASSERT_TRUE(AddNewFileInTestGallery());
  EXPECT_TRUE(gallery_change_event_received.WaitUntilSatisfied());

  // Remove gallery watch listener.
  ExecuteCmdAndCheckReply(kRemoveGalleryChangedListenerCmd,
                          kRemoveGalleryChangedListenerOK);

  // No listener, modify gallery contents.
  ASSERT_TRUE(AddNewFileInTestGallery());

  // Remove gallery watch.
  ExecuteCmdAndCheckReply(kRemoveGalleryWatchCmd, kRemoveGalleryWatchOK);
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       SetupGalleryWatchWithoutListeners) {
  if (!GalleryWatchesSupported())
    return;

  SetupGalleryWatches();

  // No listeners, modify gallery contents.
  ExtensionTestMessageListener gallery_change_event_received(
      kGalleryChangedEventReceived, false  /* no reply */);
  ASSERT_TRUE(AddNewFileInTestGallery());

  // Remove gallery watch.
  ExecuteCmdAndCheckReply(kRemoveGalleryWatchCmd, kRemoveGalleryWatchOK);
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       SetupGalleryChangedListenerWithoutWatchers) {
  // Add gallery watch listener.
  ExecuteCmdAndCheckReply(kAddGalleryChangedListenerCmd,
                          kAddGalleryChangedListenerOK);

  // Modify gallery contents. Listener should not get called because add watch
  // request was not called.
  ExtensionTestMessageListener gallery_change_event_received(
      kGalleryChangedEventReceived, false  /* no reply */);
  ASSERT_TRUE(AddNewFileInTestGallery());

  // Remove gallery watch listener.
  ExecuteCmdAndCheckReply(kRemoveGalleryChangedListenerCmd,
                          kRemoveGalleryChangedListenerOK);
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       SetupWatchOnInvalidGallery) {
  // Set up a invalid gallery watch.
  ExtensionTestMessageListener invalid_gallery_watch_request_finished(
      kAddGalleryWatchRequestFailed, false  /* no reply */);
  ExecuteCmdAndCheckReply(kSetupWatchOnInvalidGalleryCmd, kAddGalleryWatchOK);
  EXPECT_TRUE(invalid_gallery_watch_request_finished.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       GetAllGalleryWatch) {
  // Gallery watchers are not yet added.
  // chrome.mediaGalleriesPrivate.getAllGalleryWatch should return an empty
  // list.
  ExtensionTestMessageListener initial_get_all_check_finished(
      kNoGalleryWatchesInstalled, false  /* no reply */);
  ExecuteCmdAndCheckReply(kGetAllWatchedGalleryIdsCmd, kGetAllGalleryWatchOK);
  EXPECT_TRUE(initial_get_all_check_finished.WaitUntilSatisfied());

  if (!GalleryWatchesSupported())
    return;

  SetupGalleryWatches();

  // chrome.mediaGalleriesPrivate.getAllGalleryWatch should return the
  // gallery identifiers.
  ExtensionTestMessageListener get_all_watched_galleries_finished(
      kGalleryWatchesCheck, false  /* no reply */);
  ExecuteCmdAndCheckReply(kGetAllWatchedGalleryIdsCmd, kGetAllGalleryWatchOK);
  EXPECT_TRUE(get_all_watched_galleries_finished.WaitUntilSatisfied());

  // Remove gallery watch request.
  ExecuteCmdAndCheckReply(kRemoveGalleryWatchCmd, kRemoveGalleryWatchOK);

  // Gallery watchers removed.
  // chrome.mediaGalleriesPrivate.getAllGalleryWatch() should return an empty
  // list.
  ExtensionTestMessageListener final_get_all_check_finished(
      kNoGalleryWatchesInstalled, false  /* no reply */);
  ExecuteCmdAndCheckReply(kGetAllWatchedGalleryIdsCmd, kGetAllGalleryWatchOK);
  EXPECT_TRUE(final_get_all_check_finished.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPrivateGalleryWatchApiTest,
                       RemoveAllGalleryWatch) {
  if (!GalleryWatchesSupported())
    return;

  SetupGalleryWatches();

  // chrome.mediaGalleriesPrivate.getAllGalleryWatch should return the watched
  // gallery identifiers.
  ExtensionTestMessageListener get_all_watched_galleries_finished(
      kGalleryWatchesCheck, false  /* no reply */);
  ExecuteCmdAndCheckReply(kGetAllWatchedGalleryIdsCmd, kGetAllGalleryWatchOK);
  EXPECT_TRUE(get_all_watched_galleries_finished.WaitUntilSatisfied());

  // Remove all gallery watchers.
  ExecuteCmdAndCheckReply(kRemoveAllGalleryWatchCmd, kRemoveAllGalleryWatchOK);

  // Gallery watchers removed. chrome.mediaGalleriesPrivate.getAllGalleryWatch
  // should return an empty list.
  ExtensionTestMessageListener final_get_all_check_finished(
      kNoGalleryWatchesInstalled, false  /* no reply */);
  ExecuteCmdAndCheckReply(kGetAllWatchedGalleryIdsCmd, kGetAllGalleryWatchOK);
  EXPECT_TRUE(final_get_all_check_finished.WaitUntilSatisfied());
}
