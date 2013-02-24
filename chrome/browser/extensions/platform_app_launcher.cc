// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/platform_app_launcher.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/app_runtime/app_runtime_api.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/isolated_context.h"

using content::BrowserThread;
using extensions::app_file_handler_util::FileHandlerForId;
using extensions::app_file_handler_util::FileHandlerCanHandleFileWithMimeType;
using extensions::app_file_handler_util::FirstFileHandlerForMimeType;

namespace extensions {

namespace {

bool MakePathAbsolute(const base::FilePath& current_directory,
                      base::FilePath* file_path) {
  DCHECK(file_path);
  if (file_path->IsAbsolute())
    return true;

  if (current_directory.empty())
    return file_util::AbsolutePath(file_path);

  if (!current_directory.IsAbsolute())
    return false;

  *file_path = current_directory.Append(*file_path);
  return true;
}

bool GetAbsolutePathFromCommandLine(const CommandLine* command_line,
                                    const base::FilePath& current_directory,
                                    base::FilePath* path) {
  if (!command_line || !command_line->GetArgs().size())
    return false;

  base::FilePath relative_path(command_line->GetArgs()[0]);
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
  extensions::AppEventRouter::DispatchOnLaunchedEvent(profile, extension);
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
      : profile_(profile),
        extension_(extension),
        file_path_(file_path),
        handler_id_("") {}

  void Launch() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (file_path_.empty()) {
      LaunchPlatformAppWithNoData(profile_, extension_);
      return;
    }

    DCHECK(file_path_.IsAbsolute());
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
            &PlatformAppPathLauncher::GetMimeTypeAndLaunch, this));
  }

  void LaunchWithHandler(const std::string& handler_id) {
    handler_id_ = handler_id;
    Launch();
  }

 private:
  friend class base::RefCountedThreadSafe<PlatformAppPathLauncher>;

  virtual ~PlatformAppPathLauncher() {}

  void GetMimeTypeAndLaunch() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

    // If the file doesn't exist, or is a directory, launch with no launch data.
    if (!file_util::PathExists(file_path_) ||
        file_util::DirectoryExists(file_path_)) {
      LOG(WARNING) << "No file exists with path " << file_path_.value();
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
              &PlatformAppPathLauncher::LaunchWithNoLaunchData, this));
      return;
    }

    std::string mime_type;
    // If we cannot obtain the MIME type, launch with no launch data.
    if (!net::GetMimeTypeFromFile(file_path_, &mime_type)) {
      LOG(WARNING) << "Could not obtain MIME type for "
                   << file_path_.value();
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
              &PlatformAppPathLauncher::LaunchWithNoLaunchData, this));
      return;
    }

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
            &PlatformAppPathLauncher::LaunchWithMimeType, this, mime_type));
  }

  void LaunchWithNoLaunchData() {
    // This method is required as an entry point on the UI thread.
    LaunchPlatformAppWithNoData(profile_, extension_);
  }

  void LaunchWithMimeType(const std::string& mime_type) {
    // Find file handler from the platform app for the file being opened.
    const FileHandlerInfo* handler = NULL;
    if (!handler_id_.empty())
      handler = FileHandlerForId(*extension_, handler_id_);
    else
      handler = FirstFileHandlerForMimeType(*extension_, mime_type);
    if (handler &&
        !FileHandlerCanHandleFileWithMimeType(*handler, mime_type)) {
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

    // Access needs to be granted to the file for the process associated with
    // the extension. To do this the ExtensionHost is needed. This might not be
    // available, or it might be in the process of being unloaded, in which case
    // the lazy background task queue is used to load the extension and then
    // call back to us.
    LazyBackgroundTaskQueue* queue =
        ExtensionSystem::Get(profile_)->lazy_background_task_queue();
    if (queue->ShouldEnqueueTask(profile_, extension_)) {
      queue->AddPendingTask(profile_, extension_->id(), base::Bind(
              &PlatformAppPathLauncher::GrantAccessToFileAndLaunch,
              this, mime_type));
      return;
    }

    ExtensionProcessManager* process_manager =
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

    content::ChildProcessSecurityPolicy* policy =
        content::ChildProcessSecurityPolicy::GetInstance();
    int renderer_id = host->render_process_host()->GetID();

    // Granting read file permission to allow reading file content.
    // If the renderer already has permission to read these paths, it is not
    // regranted, as this would overwrite any other permissions which the
    // renderer may already have.
    if (!policy->CanReadFile(renderer_id, file_path_))
      policy->GrantReadFile(renderer_id, file_path_);

    std::string registered_name;
    fileapi::IsolatedContext* isolated_context =
        fileapi::IsolatedContext::GetInstance();
    DCHECK(isolated_context);
    std::string filesystem_id = isolated_context->RegisterFileSystemForPath(
        fileapi::kFileSystemTypeNativeLocal, file_path_, &registered_name);
    // Granting read file system permission as well to allow file-system
    // read operations.
    policy->GrantReadFileSystem(renderer_id, filesystem_id);

    AppEventRouter::DispatchOnLaunchedEventWithFileEntry(
        profile_, extension_, handler_id_, mime_type, filesystem_id,
        registered_name);
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

void LaunchPlatformApp(Profile* profile,
                       const Extension* extension,
                       const CommandLine* command_line,
                       const base::FilePath& current_directory) {
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

void LaunchPlatformAppWithFileHandler(Profile* profile,
                                      const Extension* extension,
                                      const std::string& handler_id,
                                      const base::FilePath& file_path) {
  scoped_refptr<PlatformAppPathLauncher> launcher =
      new PlatformAppPathLauncher(profile, extension, file_path);
  launcher->LaunchWithHandler(handler_id);
}

}  // namespace extensions
