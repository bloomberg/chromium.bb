// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/launcher.h"

#include "apps/browser/api/app_runtime/app_runtime_api.h"
#include "apps/browser/file_handler_util.h"
#include "apps/common/api/app_runtime.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "net/base/filename_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace app_runtime = apps::api::app_runtime;

using apps::file_handler_util::GrantedFileEntry;
using content::BrowserThread;
using extensions::app_file_handler_util::CheckWritableFiles;
using extensions::app_file_handler_util::FileHandlerForId;
using extensions::app_file_handler_util::FileHandlerCanHandleFile;
using extensions::app_file_handler_util::FirstFileHandlerForFile;
using extensions::app_file_handler_util::CreateFileEntry;
using extensions::app_file_handler_util::HasFileSystemWritePermission;
using extensions::EventRouter;
using extensions::Extension;
using extensions::ExtensionHost;
using extensions::ExtensionSystem;

namespace apps {

namespace {

const char kFallbackMimeType[] = "application/octet-stream";

bool MakePathAbsolute(const base::FilePath& current_directory,
                      base::FilePath* file_path) {
  DCHECK(file_path);
  if (file_path->IsAbsolute())
    return true;

  if (current_directory.empty()) {
    *file_path = base::MakeAbsoluteFilePath(*file_path);
    return !file_path->empty();
  }

  if (!current_directory.IsAbsolute())
    return false;

  *file_path = current_directory.Append(*file_path);
  return true;
}

bool GetAbsolutePathFromCommandLine(const CommandLine& command_line,
                                    const base::FilePath& current_directory,
                                    base::FilePath* path) {
  if (!command_line.GetArgs().size())
    return false;

  base::FilePath relative_path(command_line.GetArgs()[0]);
  base::FilePath absolute_path(relative_path);
  if (!MakePathAbsolute(current_directory, &absolute_path)) {
    LOG(WARNING) << "Cannot make absolute path from " << relative_path.value();
    return false;
  }
  *path = absolute_path;
  return true;
}

// Helper method to launch the platform app |extension| with no data. This
// should be called in the fallback case, where it has been impossible to
// load or obtain file launch data.
void LaunchPlatformAppWithNoData(Profile* profile, const Extension* extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AppEventRouter::DispatchOnLaunchedEvent(profile, extension);
}

// Class to handle launching of platform apps to open a specific path.
// An instance of this class is created for each launch. The lifetime of these
// instances is managed by reference counted pointers. As long as an instance
// has outstanding tasks on a message queue it will be retained; once all
// outstanding tasks are completed it will be deleted.
class PlatformAppPathLauncher
    : public base::RefCountedThreadSafe<PlatformAppPathLauncher> {
 public:
  PlatformAppPathLauncher(Profile* profile,
                          const Extension* extension,
                          const base::FilePath& file_path)
      : profile_(profile), extension_(extension), file_path_(file_path) {}

  void Launch() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (file_path_.empty()) {
      LaunchPlatformAppWithNoData(profile_, extension_);
      return;
    }

    DCHECK(file_path_.IsAbsolute());

    if (HasFileSystemWritePermission(extension_)) {
      std::vector<base::FilePath> paths;
      paths.push_back(file_path_);
      CheckWritableFiles(
          paths,
          profile_,
          false,
          base::Bind(&PlatformAppPathLauncher::OnFileValid, this),
          base::Bind(&PlatformAppPathLauncher::OnFileInvalid, this));
      return;
    }

    OnFileValid();
  }

  void LaunchWithHandler(const std::string& handler_id) {
    handler_id_ = handler_id;
    Launch();
  }

 private:
  friend class base::RefCountedThreadSafe<PlatformAppPathLauncher>;

  virtual ~PlatformAppPathLauncher() {}

  void OnFileValid() {
#if defined(OS_CHROMEOS)
    if (drive::util::IsUnderDriveMountPoint(file_path_)) {
      PlatformAppPathLauncher::GetMimeTypeAndLaunchForDriveFile();
      return;
    }
#endif

    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&PlatformAppPathLauncher::GetMimeTypeAndLaunch, this));
  }

  void OnFileInvalid(const base::FilePath& /* error_path */) {
    LaunchWithNoLaunchData();
  }

  void GetMimeTypeAndLaunch() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

    // If the file doesn't exist, or is a directory, launch with no launch data.
    if (!base::PathExists(file_path_) ||
        base::DirectoryExists(file_path_)) {
      LOG(WARNING) << "No file exists with path " << file_path_.value();
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
              &PlatformAppPathLauncher::LaunchWithNoLaunchData, this));
      return;
    }

    std::string mime_type;
    if (!net::GetMimeTypeFromFile(file_path_, &mime_type)) {
      // If MIME type of the file can't be determined by its path,
      // try to sniff it by its content.
      std::vector<char> content(net::kMaxBytesToSniff);
      int bytes_read = base::ReadFile(file_path_, &content[0], content.size());
      if (bytes_read >= 0) {
        net::SniffMimeType(&content[0],
                           bytes_read,
                           net::FilePathToFileURL(file_path_),
                           std::string(),  // type_hint (passes no hint)
                           &mime_type);
      }
      if (mime_type.empty())
        mime_type = kFallbackMimeType;
    }

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
            &PlatformAppPathLauncher::LaunchWithMimeType, this, mime_type));
  }

#if defined(OS_CHROMEOS)
  void GetMimeTypeAndLaunchForDriveFile() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    drive::FileSystemInterface* file_system =
        drive::util::GetFileSystemByProfile(profile_);
    if (!file_system) {
      LaunchWithNoLaunchData();
      return;
    }

    file_system->GetFile(
        drive::util::ExtractDrivePath(file_path_),
        base::Bind(&PlatformAppPathLauncher::OnGotDriveFile, this));
  }

  void OnGotDriveFile(drive::FileError error,
                      const base::FilePath& file_path,
                      scoped_ptr<drive::ResourceEntry> entry) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    if (error != drive::FILE_ERROR_OK ||
        !entry || entry->file_specific_info().is_hosted_document()) {
      LaunchWithNoLaunchData();
      return;
    }

    const std::string& mime_type =
        entry->file_specific_info().content_mime_type();
    LaunchWithMimeType(mime_type.empty() ? kFallbackMimeType : mime_type);
  }
#endif  // defined(OS_CHROMEOS)

  void LaunchWithNoLaunchData() {
    // This method is required as an entry point on the UI thread.
    LaunchPlatformAppWithNoData(profile_, extension_);
  }

  void LaunchWithMimeType(const std::string& mime_type) {
    // Find file handler from the platform app for the file being opened.
    const extensions::FileHandlerInfo* handler = NULL;
    if (!handler_id_.empty())
      handler = FileHandlerForId(*extension_, handler_id_);
    else
      handler = FirstFileHandlerForFile(*extension_, mime_type, file_path_);
    if (handler && !FileHandlerCanHandleFile(*handler, mime_type, file_path_)) {
      LOG(WARNING) << "Extension does not provide a valid file handler for "
                   << file_path_.value();
      LaunchWithNoLaunchData();
      return;
    }

    // If this app doesn't have a file handler that supports the file, launch
    // with no launch data.
    if (!handler) {
      LOG(WARNING) << "Extension does not provide a valid file handler for "
                   << file_path_.value();
      LaunchWithNoLaunchData();
      return;
    }

    if (handler_id_.empty())
      handler_id_ = handler->id;

    // Access needs to be granted to the file for the process associated with
    // the extension. To do this the ExtensionHost is needed. This might not be
    // available, or it might be in the process of being unloaded, in which case
    // the lazy background task queue is used to load the extension and then
    // call back to us.
    extensions::LazyBackgroundTaskQueue* queue =
        ExtensionSystem::Get(profile_)->lazy_background_task_queue();
    if (queue->ShouldEnqueueTask(profile_, extension_)) {
      queue->AddPendingTask(profile_, extension_->id(), base::Bind(
              &PlatformAppPathLauncher::GrantAccessToFileAndLaunch,
              this, mime_type));
      return;
    }

    extensions::ProcessManager* process_manager =
        ExtensionSystem::Get(profile_)->process_manager();
    ExtensionHost* host =
        process_manager->GetBackgroundHostForExtension(extension_->id());
    DCHECK(host);
    GrantAccessToFileAndLaunch(mime_type, host);
  }

  void GrantAccessToFileAndLaunch(const std::string& mime_type,
                                  ExtensionHost* host) {
    // If there was an error loading the app page, |host| will be NULL.
    if (!host) {
      LOG(ERROR) << "Could not load app page for " << extension_->id();
      return;
    }

    GrantedFileEntry file_entry =
        CreateFileEntry(profile_,
                        extension_,
                        host->render_process_host()->GetID(),
                        file_path_,
                        false);
    AppEventRouter::DispatchOnLaunchedEventWithFileEntry(
        profile_, extension_, handler_id_, mime_type, file_entry);
  }

  // The profile the app should be run in.
  Profile* profile_;
  // The extension providing the app.
  const Extension* extension_;
  // The path to be passed through to the app.
  const base::FilePath file_path_;
  // The ID of the file handler used to launch the app.
  std::string handler_id_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAppPathLauncher);
};

}  // namespace

void LaunchPlatformAppWithCommandLine(Profile* profile,
                                      const Extension* extension,
                                      const CommandLine& command_line,
                                      const base::FilePath& current_directory) {
  // An app with "kiosk_only" should not be installed and launched
  // outside of ChromeOS kiosk mode in the first place. This is a defensive
  // check in case this scenario does occur.
  if (extensions::KioskModeInfo::IsKioskOnly(extension)) {
    bool in_kiosk_mode = false;
#if defined(OS_CHROMEOS)
    chromeos::UserManager* user_manager = chromeos::UserManager::Get();
    in_kiosk_mode = user_manager && user_manager->IsLoggedInAsKioskApp();
#endif
    if (!in_kiosk_mode) {
      LOG(ERROR) << "App with 'kiosk_only' attribute must be run in "
          << " ChromeOS kiosk mode.";
      NOTREACHED();
      return;
    }
  }

  base::FilePath path;
  if (!GetAbsolutePathFromCommandLine(command_line, current_directory, &path)) {
    LaunchPlatformAppWithNoData(profile, extension);
    return;
  }

  // TODO(benwells): add a command-line argument to provide a handler ID.
  LaunchPlatformAppWithPath(profile, extension, path);
}

void LaunchPlatformAppWithPath(Profile* profile,
                               const Extension* extension,
                               const base::FilePath& file_path) {
  // launcher will be freed when nothing has a reference to it. The message
  // queue will retain a reference for any outstanding task, so when the
  // launcher has finished it will be freed.
  scoped_refptr<PlatformAppPathLauncher> launcher =
      new PlatformAppPathLauncher(profile, extension, file_path);
  launcher->Launch();
}

void LaunchPlatformApp(Profile* profile, const Extension* extension) {
  LaunchPlatformAppWithCommandLine(profile,
                                   extension,
                                   CommandLine(CommandLine::NO_PROGRAM),
                                   base::FilePath());
}

void LaunchPlatformAppWithFileHandler(Profile* profile,
                                      const Extension* extension,
                                      const std::string& handler_id,
                                      const base::FilePath& file_path) {
  scoped_refptr<PlatformAppPathLauncher> launcher =
      new PlatformAppPathLauncher(profile, extension, file_path);
  launcher->LaunchWithHandler(handler_id);
}

void RestartPlatformApp(Profile* profile, const Extension* extension) {
  EventRouter* event_router = EventRouter::Get(profile);
  bool listening_to_restart = event_router->
      ExtensionHasEventListener(extension->id(),
                                app_runtime::OnRestarted::kEventName);

  if (listening_to_restart) {
    AppEventRouter::DispatchOnRestartedEvent(profile, extension);
    return;
  }

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile);
  bool had_windows = extension_prefs->IsActive(extension->id());
  extension_prefs->SetIsActive(extension->id(), false);
  bool listening_to_launch = event_router->
      ExtensionHasEventListener(extension->id(),
                                app_runtime::OnLaunched::kEventName);

  if (listening_to_launch && had_windows)
    LaunchPlatformAppWithNoData(profile, extension);
}

void LaunchPlatformAppWithUrl(Profile* profile,
                              const Extension* extension,
                              const std::string& handler_id,
                              const GURL& url,
                              const GURL& referrer_url) {
  AppEventRouter::DispatchOnLaunchedEventWithUrl(
      profile, extension, handler_id, url, referrer_url);
}

}  // namespace apps
