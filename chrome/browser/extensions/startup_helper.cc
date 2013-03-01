// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/startup_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"

namespace {

void PrintPackExtensionMessage(const std::string& message) {
  printf("%s\n", message.c_str());
}

}  // namespace

namespace extensions {

StartupHelper::StartupHelper() : pack_job_succeeded_(false) {
  (new DefaultLocaleHandler)->Register();
}

void StartupHelper::OnPackSuccess(
    const base::FilePath& crx_path,
    const base::FilePath& output_private_key_path) {
  pack_job_succeeded_ = true;
  PrintPackExtensionMessage(
      UTF16ToUTF8(
          PackExtensionJob::StandardSuccessMessage(crx_path,
                                                   output_private_key_path)));
}

void StartupHelper::OnPackFailure(const std::string& error_message,
                                  ExtensionCreator::ErrorType type) {
  PrintPackExtensionMessage(error_message);
}

bool StartupHelper::PackExtension(const CommandLine& cmd_line) {
  if (!cmd_line.HasSwitch(switches::kPackExtension))
    return false;

  // Input Paths.
  base::FilePath src_dir =
      cmd_line.GetSwitchValuePath(switches::kPackExtension);
  base::FilePath private_key_path;
  if (cmd_line.HasSwitch(switches::kPackExtensionKey)) {
    private_key_path = cmd_line.GetSwitchValuePath(switches::kPackExtensionKey);
  }

  // Launch a job to perform the packing on the file thread.  Ignore warnings
  // from the packing process. (e.g. Overwrite any existing crx file.)
  pack_job_ = new PackExtensionJob(this, src_dir, private_key_path,
                                   ExtensionCreator::kOverwriteCRX);
  pack_job_->set_asynchronous(false);
  pack_job_->Start();

  return pack_job_succeeded_;
}

bool StartupHelper::UninstallExtension(const CommandLine& cmd_line,
                                       Profile* profile) {
  DCHECK(profile);

  if (!cmd_line.HasSwitch(switches::kUninstallExtension))
    return false;

  ExtensionService* extension_service = profile->GetExtensionService();
  if (!extension_service)
    return false;

  std::string extension_id = cmd_line.GetSwitchValueASCII(
      switches::kUninstallExtension);
  return ExtensionService::UninstallExtensionHelper(extension_service,
                                                    extension_id);
}

namespace {

class AppInstallHelper {
 public:
  // A callback for when the install process is done.
  typedef base::Callback<void()> DoneCallback;

  AppInstallHelper();
  virtual ~AppInstallHelper();
  bool success() { return success_; }
  const std::string& error() { return error_; }
  void BeginInstall(Profile* profile,
                    const std::string& id,
                    WebstoreStandaloneInstaller::PromptType prompt_type,
                    DoneCallback callback);

 private:
  WebstoreStandaloneInstaller::Callback Callback();
  void OnAppInstallComplete(bool success, const std::string& error);

  DoneCallback done_callback_;

  // These hold on to the result of the app install when it is complete.
  bool success_;
  std::string error_;

  scoped_ptr<content::WebContents> web_contents_;
  scoped_refptr<WebstoreStandaloneInstaller> installer_;
};

AppInstallHelper::AppInstallHelper() : success_(false) {}

AppInstallHelper::~AppInstallHelper() {}

WebstoreStandaloneInstaller::Callback AppInstallHelper::Callback() {
  return base::Bind(&AppInstallHelper::OnAppInstallComplete,
                    base::Unretained(this));
}

void AppInstallHelper::BeginInstall(
    Profile* profile,
    const std::string& id,
    WebstoreStandaloneInstaller::PromptType prompt_type,
    DoneCallback done_callback) {
  done_callback_ = done_callback;

  WebstoreStandaloneInstaller::Callback callback =
      base::Bind(&AppInstallHelper::OnAppInstallComplete,
                 base::Unretained(this));
  installer_ = new WebstoreStandaloneInstaller(
      id,
      WebstoreStandaloneInstaller::DO_NOT_REQUIRE_VERIFIED_SITE,
      prompt_type,
      GURL(),
      profile,
      NULL,
      callback);
  installer_->BeginInstall();
}

void AppInstallHelper::OnAppInstallComplete(bool success,
                                            const std::string& error) {
  success_ = success;
  error_= error;
  done_callback_.Run();
}

void DeleteHelperAndRunCallback(AppInstallHelper* helper,
                                base::Callback<void()> callback) {
  delete helper;
  callback.Run();
}

}  // namespace

bool StartupHelper::InstallFromWebstore(const CommandLine& cmd_line,
                                        Profile* profile) {
  std::string id = cmd_line.GetSwitchValueASCII(switches::kInstallFromWebstore);
  if (!Extension::IdIsValid(id)) {
    LOG(ERROR) << "Invalid id for " << switches::kInstallFromWebstore
               << " : '" << id << "'";
    return false;
  }

  AppInstallHelper helper;
  helper.BeginInstall(profile, id,
      cmd_line.HasSwitch(switches::kForceAppMode) ?
          WebstoreStandaloneInstaller::SKIP_PROMPT :
          WebstoreStandaloneInstaller::STANDARD_PROMPT,
      MessageLoop::QuitWhenIdleClosure());

  MessageLoop::current()->Run();
  if (!helper.success())
    LOG(ERROR) << "InstallFromWebstore failed with error: " << helper.error();
  return helper.success();
}

void StartupHelper::LimitedInstallFromWebstore(
    const CommandLine& cmd_line,
    Profile* profile,
    base::Callback<void()> done_callback) {
  std::string id = WebStoreIdFromLimitedInstallCmdLine(cmd_line);
  if (!Extension::IdIsValid(id)) {
    LOG(ERROR) << "Invalid index for " << switches::kLimitedInstallFromWebstore;
    done_callback.Run();
    return;
  }

  AppInstallHelper* helper = new AppInstallHelper();
  helper->BeginInstall(profile, id,
      WebstoreStandaloneInstaller::STANDARD_PROMPT,
      base::Bind(&DeleteHelperAndRunCallback, helper, done_callback));
}

std::string StartupHelper::WebStoreIdFromLimitedInstallCmdLine(
    const CommandLine& cmd_line) {
  std::string index = cmd_line.GetSwitchValueASCII(
      switches::kLimitedInstallFromWebstore);
  std::string id;
  if (index == "1") {
    id = "nckgahadagoaajjgafhacjanaoiihapd";
  } else if (index == "2") {
    id = "ecglahbcnmdpdciemllbhojghbkagdje";
  }
  return id;
}

StartupHelper::~StartupHelper() {
  if (pack_job_.get())
    pack_job_->ClearClient();
}

}  // namespace extensions
