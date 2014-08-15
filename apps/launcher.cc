// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/launcher.h"

#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/api/file_handlers/mime_util.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "net/base/filename_util.h"
#include "net/base/net_util.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

namespace app_runtime = extensions::core_api::app_runtime;

using content::BrowserThread;
using extensions::AppRuntimeEventRouter;
using extensions::app_file_handler_util::CreateFileEntry;
using extensions::app_file_handler_util::FileHandlerCanHandleFile;
using extensions::app_file_handler_util::FileHandlerForId;
using extensions::app_file_handler_util::FirstFileHandlerForFile;
using extensions::app_file_handler_util::HasFileSystemWritePermission;
using extensions::app_file_handler_util::PrepareFilesForWritableApp;
using extensions::EventRouter;
using extensions::Extension;
using extensions::ExtensionHost;
using extensions::ExtensionSystem;
using extensions::GrantedFileEntry;

namespace apps {

namespace {

const char kFallbackMimeType[] = "application/octet-stream";

bool DoMakePathAbsolute(const base::FilePath& current_directory,
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

// Helper method to launch the platform app |extension| with no data. This
// should be called in the fallback case, where it has been impossible to
// load or obtain file launch data.
void LaunchPlatformAppWithNoData(Profile* profile, const Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AppRuntimeEventRouter::DispatchOnLaunchedEvent(profile, extension);
}

// Class to handle launching of platform apps to open specific paths.
// An instance of this class is created for each launch. The lifetime of these
// instances is managed by reference counted pointers. As long as an instance
// has outstanding tasks on a message queue it will be retained; once all
// outstanding tasks are completed it will be deleted.
class PlatformAppPathLauncher
    : public base::RefCountedThreadSafe<PlatformAppPathLauncher> {
 public:
  PlatformAppPathLauncher(Profile* profile,
                          const Extension* extension,
                          const std::vector<base::FilePath>& file_paths)
      : profile_(profile),
        extension_(extension),
        file_paths_(file_paths),
        collector_(profile) {}

  PlatformAppPathLauncher(Profile* profile,
                          const Extension* extension,
                          const base::FilePath& file_path)
      : profile_(profile), extension_(extension), collector_(profile) {
    if (!file_path.empty())
      file_paths_.push_back(file_path);
  }

  void Launch() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (file_paths_.empty()) {
      LaunchPlatformAppWithNoData(profile_, extension_);
      return;
    }

    for (size_t i = 0; i < file_paths_.size(); ++i) {
      DCHECK(file_paths_[i].IsAbsolute());
    }

    if (HasFileSystemWritePermission(extension_)) {
      PrepareFilesForWritableApp(
          file_paths_,
          profile_,
          false,
          base::Bind(&PlatformAppPathLauncher::OnFilesValid, this),
          base::Bind(&PlatformAppPathLauncher::OnFilesInvalid, this));
      return;
    }

    OnFilesValid();
  }

  void LaunchWithHandler(const std::string& handler_id) {
    handler_id_ = handler_id;
    Launch();
  }

  void LaunchWithRelativePath(const base::FilePath& current_directory) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&PlatformAppPathLauncher::MakePathAbsolute,
                   this,
                   current_directory));
  }

 private:
  friend class base::RefCountedThreadSafe<PlatformAppPathLauncher>;

  virtual ~PlatformAppPathLauncher() {}

  void MakePathAbsolute(const base::FilePath& current_directory) {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);

    for (std::vector<base::FilePath>::iterator it = file_paths_.begin();
         it != file_paths_.end();
         ++it) {
      if (!DoMakePathAbsolute(current_directory, &*it)) {
        LOG(WARNING) << "Cannot make absolute path from " << it->value();
        BrowserThread::PostTask(
            BrowserThread::UI,
            FROM_HERE,
            base::Bind(&PlatformAppPathLauncher::LaunchWithNoLaunchData, this));
        return;
      }
    }

    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&PlatformAppPathLauncher::Launch, this));
  }

  void OnFilesValid() {
    collector_.CollectForLocalPaths(
        file_paths_,
        base::Bind(&PlatformAppPathLauncher::OnMimeTypesCollected, this));
  }

  void OnFilesInvalid(const base::FilePath& /* error_path */) {
    LaunchWithNoLaunchData();
  }

  void LaunchWithNoLaunchData() {
    // This method is required as an entry point on the UI thread.
    LaunchPlatformAppWithNoData(profile_, extension_);
  }

  void OnMimeTypesCollected(scoped_ptr<std::vector<std::string> > mime_types) {
    DCHECK(file_paths_.size() == mime_types->size());

    // If fetching a mime type failed, then use a fallback one.
    for (size_t i = 0; i < mime_types->size(); ++i) {
      const std::string mime_type =
          !(*mime_types)[i].empty() ? (*mime_types)[i] : kFallbackMimeType;
      mime_types_.push_back(mime_type);
    }

    // Find file handler from the platform app for the file being opened.
    const extensions::FileHandlerInfo* handler = NULL;
    if (!handler_id_.empty()) {
      handler = FileHandlerForId(*extension_, handler_id_);
      if (handler) {
        for (size_t i = 0; i < file_paths_.size(); ++i) {
          if (!FileHandlerCanHandleFile(
                  *handler, mime_types_[i], file_paths_[i])) {
            LOG(WARNING)
                << "Extension does not provide a valid file handler for "
                << file_paths_[i].value();
            handler = NULL;
            break;
          }
        }
      }
    } else {
      std::set<std::pair<base::FilePath, std::string> > path_and_file_type_set;
      for (size_t i = 0; i < file_paths_.size(); ++i) {
        path_and_file_type_set.insert(
            std::make_pair(file_paths_[i], mime_types_[i]));
      }
      const std::vector<const extensions::FileHandlerInfo*>& handlers =
          extensions::app_file_handler_util::FindFileHandlersForFiles(
              *extension_, path_and_file_type_set);
      if (!handlers.empty())
        handler = handlers[0];
    }

    // If this app doesn't have a file handler that supports the file, launch
    // with no launch data.
    if (!handler) {
      LOG(WARNING) << "Extension does not provide a valid file handler.";
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
    extensions::LazyBackgroundTaskQueue* const queue =
        ExtensionSystem::Get(profile_)->lazy_background_task_queue();
    if (queue->ShouldEnqueueTask(profile_, extension_)) {
      queue->AddPendingTask(
          profile_,
          extension_->id(),
          base::Bind(&PlatformAppPathLauncher::GrantAccessToFilesAndLaunch,
                     this));
      return;
    }

    extensions::ProcessManager* const process_manager =
        ExtensionSystem::Get(profile_)->process_manager();
    ExtensionHost* const host =
        process_manager->GetBackgroundHostForExtension(extension_->id());
    DCHECK(host);
    GrantAccessToFilesAndLaunch(host);
  }

  void GrantAccessToFilesAndLaunch(ExtensionHost* host) {
    // If there was an error loading the app page, |host| will be NULL.
    if (!host) {
      LOG(ERROR) << "Could not load app page for " << extension_->id();
      return;
    }

    std::vector<GrantedFileEntry> file_entries;
    for (size_t i = 0; i < file_paths_.size(); ++i) {
      file_entries.push_back(
          CreateFileEntry(profile_,
                          extension_,
                          host->render_process_host()->GetID(),
                          file_paths_[i],
                          false));
    }

    AppRuntimeEventRouter::DispatchOnLaunchedEventWithFileEntries(
        profile_, extension_, handler_id_, mime_types_, file_entries);
  }

  // The profile the app should be run in.
  Profile* profile_;
  // The extension providing the app.
  // TODO(benwells): Hold onto the extension ID instead of a pointer as it
  // is possible the extension will be unloaded while we're doing our thing.
  // See http://crbug.com/372270 for details.
  const Extension* extension_;
  // The path to be passed through to the app.
  std::vector<base::FilePath> file_paths_;
  std::vector<std::string> mime_types_;
  // The ID of the file handler used to launch the app.
  std::string handler_id_;
  extensions::app_file_handler_util::MimeTypeCollector collector_;

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
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    in_kiosk_mode = user_manager && user_manager->IsLoggedInAsKioskApp();
#endif
    if (!in_kiosk_mode) {
      LOG(ERROR) << "App with 'kiosk_only' attribute must be run in "
          << " ChromeOS kiosk mode.";
      NOTREACHED();
      return;
    }
  }

#if defined(OS_WIN)
  base::CommandLine::StringType about_blank_url(
      base::ASCIIToWide(url::kAboutBlankURL));
#else
  base::CommandLine::StringType about_blank_url(url::kAboutBlankURL);
#endif
  CommandLine::StringVector args = command_line.GetArgs();
  // Browser tests will add about:blank to the command line. This should
  // never be interpreted as a file to open, as doing so with an app that
  // has write access will result in a file 'about' being created, which
  // causes problems on the bots.
  if (args.empty() || (command_line.HasSwitch(switches::kTestType) &&
                       args[0] == about_blank_url)) {
    LaunchPlatformAppWithNoData(profile, extension);
    return;
  }

  base::FilePath file_path(command_line.GetArgs()[0]);
  scoped_refptr<PlatformAppPathLauncher> launcher =
      new PlatformAppPathLauncher(profile, extension, file_path);
  launcher->LaunchWithRelativePath(current_directory);
}

void LaunchPlatformAppWithPath(Profile* profile,
                               const Extension* extension,
                               const base::FilePath& file_path) {
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

void LaunchPlatformAppWithFileHandler(
    Profile* profile,
    const Extension* extension,
    const std::string& handler_id,
    const std::vector<base::FilePath>& file_paths) {
  scoped_refptr<PlatformAppPathLauncher> launcher =
      new PlatformAppPathLauncher(profile, extension, file_paths);
  launcher->LaunchWithHandler(handler_id);
}

void RestartPlatformApp(Profile* profile, const Extension* extension) {
  EventRouter* event_router = EventRouter::Get(profile);
  bool listening_to_restart = event_router->
      ExtensionHasEventListener(extension->id(),
                                app_runtime::OnRestarted::kEventName);

  if (listening_to_restart) {
    AppRuntimeEventRouter::DispatchOnRestartedEvent(profile, extension);
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
  AppRuntimeEventRouter::DispatchOnLaunchedEventWithUrl(
      profile, extension, handler_id, url, referrer_url);
}

}  // namespace apps
