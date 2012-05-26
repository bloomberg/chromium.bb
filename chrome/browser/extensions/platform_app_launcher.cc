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
#include "webkit/glue/web_intent_service_data.h"

using content::BrowserThread;
using extensions::Extension;

namespace {

const char kViewIntent[] = "http://webintents.org/view";

class PlatformAppLauncher
    : public base::RefCountedThreadSafe<PlatformAppLauncher> {
 public:
  PlatformAppLauncher(Profile* profile,
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
        &PlatformAppLauncher::GetMimeTypeAndLaunch, this, file_path));
  }

 private:
  friend class base::RefCountedThreadSafe<PlatformAppLauncher>;

  virtual ~PlatformAppLauncher() {}

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
          &PlatformAppLauncher::LaunchWithNoLaunchData, this));
      return;
    }

    std::string mime_type;
    // If we cannot obtain the MIME type, launch with no launch data.
    if (!net::GetMimeTypeFromFile(file_path, &mime_type)) {
      LOG(WARNING) << "Could not obtain MIME type for " << file_path.value();
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
          &PlatformAppLauncher::LaunchWithNoLaunchData, this));
      return;
    }

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &PlatformAppLauncher::LaunchWithMimeTypeAndPath, this, file_path,
        mime_type));
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

    // We need to grant access to the file for the process associated with the
    // extension. To do this we need the ExtensionHost. This might not be
    // available, or it might be in the process of being unloaded, in which case
    // we can use the lazy background task queue to load the extension and then
    // call back to us.
    extensions::LazyBackgroundTaskQueue* queue =
        ExtensionSystem::Get(profile_)->lazy_background_task_queue();
    if (queue->ShouldEnqueueTask(profile_, extension_)) {
      queue->AddPendingTask(profile_, extension_->id(),
          base::Bind(&PlatformAppLauncher::GrantAccessToFileAndLaunch,
              this, file_path, mime_type));
      return;
    }

    ExtensionProcessManager* pm =
        ExtensionSystem::Get(profile_)->process_manager();
    ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_->id());
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

    // If the renderer already has permission to read these paths, we don't
    // regrant, as this would overwrite any other permissions which the renderer
    // may already have.
    if (!policy->CanReadFile(renderer_id, file_path))
      policy->GrantReadFile(renderer_id, file_path);

    std::set<FilePath> filesets;
    filesets.insert(file_path);

    fileapi::IsolatedContext* isolated_context =
        fileapi::IsolatedContext::GetInstance();
    DCHECK(isolated_context);
    std::string filesystem_id = isolated_context->RegisterIsolatedFileSystem(
        filesets);
    policy->GrantAccessFileSystem(renderer_id, filesystem_id);

    extensions::AppEventRouter::DispatchOnLaunchedEventWithFileEntry(
        profile_, extension_, ASCIIToUTF16(kViewIntent), filesystem_id,
        file_path.BaseName());
  }

  Profile* profile_;
  const Extension* extension_;
  const CommandLine* command_line_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAppLauncher);
};

}  // namespace

namespace extensions {

void LaunchPlatformApp(Profile* profile,
                       const Extension* extension,
                       const CommandLine* command_line) {
  // launcher will be freed when nothing has a reference to it. The message
  // queue will retain a reference for any outstanding task, so when the
  // launcher has finished it will be freed.
  scoped_refptr<PlatformAppLauncher> launcher =
      new PlatformAppLauncher(profile, extension, command_line);
  launcher->Launch();
}

}  // namespace extensions
