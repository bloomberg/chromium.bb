// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/platform_app_launcher.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/app/app_api.h"
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
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_service_data.h"

using content::BrowserThread;
using extensions::Extension;

namespace {

const char kViewIntent[] = "http://webintents.org/view";

// Class to handle launching of platform apps with command line information.
// An instance of this class is created for each launch. The lifetime of these
// instances is managed by reference counted pointers. As long as an instance
// has outstanding tasks on a message queue it will be retained; once all
// outstanding tasks are completed it will be deleted.
class PlatformAppCommandLineLauncher
    : public base::RefCountedThreadSafe<PlatformAppCommandLineLauncher> {
 public:
  PlatformAppCommandLineLauncher(Profile* profile,
                                 const Extension* extension,
                                 const CommandLine* command_line)
      : profile_(profile),
        extension_(extension),
        command_line_(command_line) {}

  void Launch() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!command_line_ || !command_line_->GetArgs().size()) {
      LaunchWithNoLaunchData();
      return;
    }

    FilePath file_path(command_line_->GetArgs()[0]);
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
            &PlatformAppCommandLineLauncher::GetMimeTypeAndLaunch,
            this, file_path));
  }

 private:
  friend class base::RefCountedThreadSafe<PlatformAppCommandLineLauncher>;

  virtual ~PlatformAppCommandLineLauncher() {}

  void LaunchWithNoLaunchData() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    extensions::AppEventRouter::DispatchOnLaunchedEvent(profile_, extension_);
  }

  void GetMimeTypeAndLaunch(const FilePath& file_path) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

    // If the file doesn't exist, or is a directory, launch with no launch data.
    if (!file_util::PathExists(file_path) ||
        file_util::DirectoryExists(file_path)) {
      LOG(WARNING) << "No file exists with path " << file_path.value();
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
              &PlatformAppCommandLineLauncher::LaunchWithNoLaunchData, this));
      return;
    }

    std::string mime_type;
    // If we cannot obtain the MIME type, launch with no launch data.
    if (!net::GetMimeTypeFromFile(file_path, &mime_type)) {
      LOG(WARNING) << "Could not obtain MIME type for " << file_path.value();
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
              &PlatformAppCommandLineLauncher::LaunchWithNoLaunchData, this));
      return;
    }

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
            &PlatformAppCommandLineLauncher::LaunchWithMimeTypeAndPath,
            this, file_path, mime_type));
  }

  void LaunchWithMimeTypeAndPath(const FilePath& file_path,
                                 const std::string& mime_type) {
    // Find the intent service from the platform app for the file being opened.
    webkit_glue::WebIntentServiceData service;
    bool found_service = false;

    std::vector<webkit_glue::WebIntentServiceData> services =
        extension_->intents_services();
    for (size_t i = 0; i < services.size(); i++) {
      std::string service_type_ascii = UTF16ToASCII(services[i].type);
      if (services[i].action == ASCIIToUTF16(kViewIntent) &&
          net::MatchesMimeType(service_type_ascii, mime_type)) {
        service = services[i];
        found_service = true;
        break;
      }
    }

    // If this app doesn't have an intent that supports the file, launch with
    // no launch data.
    if (!found_service) {
      LOG(WARNING) << "Extension does not provide a valid intent for "
                   << file_path.value();
      LaunchWithNoLaunchData();
      return;
    }

    // Access needs to be granted to the file for the process associated with
    // the extension. To do this the ExtensionHost is needed. This might not be
    // available, or it might be in the process of being unloaded, in which case
    // the lazy background task queue is used to load the extension and then
    // call back to us.
    extensions::LazyBackgroundTaskQueue* queue =
        ExtensionSystem::Get(profile_)->lazy_background_task_queue();
    if (queue->ShouldEnqueueTask(profile_, extension_)) {
      queue->AddPendingTask(profile_, extension_->id(), base::Bind(
              &PlatformAppCommandLineLauncher::GrantAccessToFileAndLaunch,
              this, file_path, mime_type));
      return;
    }

    ExtensionProcessManager* process_manager =
        ExtensionSystem::Get(profile_)->process_manager();
    ExtensionHost* host =
        process_manager->GetBackgroundHostForExtension(extension_->id());
    DCHECK(host);
    GrantAccessToFileAndLaunch(file_path, mime_type, host);
  }

  void GrantAccessToFileAndLaunch(const FilePath& file_path,
                                  const std::string& mime_type,
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
    if (!policy->CanReadFile(renderer_id, file_path))
      policy->GrantReadFile(renderer_id, file_path);

    std::set<FilePath> filesets;
    filesets.insert(file_path);

    fileapi::IsolatedContext* isolated_context =
        fileapi::IsolatedContext::GetInstance();
    DCHECK(isolated_context);
    std::string filesystem_id = isolated_context->RegisterIsolatedFileSystem(
        filesets);
    // Granting read file system permission as well to allow file-system
    // read operations.
    policy->GrantReadFileSystem(renderer_id, filesystem_id);

    extensions::AppEventRouter::DispatchOnLaunchedEventWithFileEntry(
        profile_, extension_, ASCIIToUTF16(kViewIntent), filesystem_id,
        file_path.BaseName());
  }

  // The profile the app should be run in.
  Profile* profile_;
  // The extension providing the app.
  const Extension* extension_;
  // The command line to be passed through to the app, or NULL.
  const CommandLine* command_line_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAppCommandLineLauncher);
};

// Class to handle launching of platform apps with WebIntent data that is being
// passed in a a blob.
// An instance of this class is created for each launch. The lifetime of these
// instances is managed by reference counted pointers. As long as an instance
// has outstanding tasks on a message queue it will be retained; once all
// outstanding tasks are completed it will be deleted.
class PlatformAppBlobIntentLauncher
    : public base::RefCountedThreadSafe<PlatformAppBlobIntentLauncher> {
 public:
  PlatformAppBlobIntentLauncher(Profile* profile,
                                const Extension* extension,
                                const webkit_glue::WebIntentData& data)
      : profile_(profile),
        extension_(extension),
        data_(data) {}

  void Launch() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // Access needs to be granted to the file for the process associated with
    // the extension. To do this the ExtensionHost is needed. This might not be
    // available, or it might be in the process of being unloaded, in which case
    // the lazy background task queue is used to load the extension and then
    // call back to us.
    extensions::LazyBackgroundTaskQueue* queue =
        ExtensionSystem::Get(profile_)->lazy_background_task_queue();
    if (queue->ShouldEnqueueTask(profile_, extension_)) {
      queue->AddPendingTask(profile_, extension_->id(), base::Bind(
              &PlatformAppBlobIntentLauncher::GrantAccessToFileAndLaunch,
              this));
      return;
    }

    ExtensionProcessManager* process_manager =
        ExtensionSystem::Get(profile_)->process_manager();
    ExtensionHost* host =
        process_manager->GetBackgroundHostForExtension(extension_->id());
    DCHECK(host);
    GrantAccessToFileAndLaunch(host);
  }

 private:
  friend class base::RefCountedThreadSafe<PlatformAppBlobIntentLauncher>;

  virtual ~PlatformAppBlobIntentLauncher() {}

  void GrantAccessToFileAndLaunch(ExtensionHost* host) {
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
    if (!policy->CanReadFile(renderer_id, data_.blob_file))
      policy->GrantReadFile(renderer_id, data_.blob_file);

    extensions::AppEventRouter::DispatchOnLaunchedEventWithWebIntent(
        profile_, extension_, data_);
  }

  // The profile the app should be run in.
  Profile* profile_;
  // The extension providing the app.
  const Extension* extension_;
  // The WebIntent data to be passed through to the app.
  const webkit_glue::WebIntentData data_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAppBlobIntentLauncher);
};

}  // namespace

namespace extensions {

void LaunchPlatformApp(Profile* profile,
                       const Extension* extension,
                       const CommandLine* command_line) {
  // TODO(benwells): remove this logging.
  LOG(WARNING) << "Launching platform app with command line data.";
  // launcher will be freed when nothing has a reference to it. The message
  // queue will retain a reference for any outstanding task, so when the
  // launcher has finished it will be freed.
  scoped_refptr<PlatformAppCommandLineLauncher> launcher =
      new PlatformAppCommandLineLauncher(profile, extension, command_line);
  launcher->Launch();
}

void LaunchPlatformAppWithWebIntent(
    Profile* profile,
    const Extension* extension,
    const webkit_glue::WebIntentData& web_intent_data) {
  // TODO(benwells): remove this logging.
  LOG(WARNING) << "Launching platform app with WebIntent.";
  if (web_intent_data.data_type == webkit_glue::WebIntentData::BLOB) {
    scoped_refptr<PlatformAppBlobIntentLauncher> launcher =
        new PlatformAppBlobIntentLauncher(profile, extension, web_intent_data);
    launcher->Launch();
    return;
  }

  extensions::AppEventRouter::DispatchOnLaunchedEventWithWebIntent(
      profile, extension, web_intent_data);
}

}  // namespace extensions
