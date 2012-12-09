// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/startup_helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"

namespace {

void PrintPackExtensionMessage(const std::string& message) {
  printf("%s\n", message.c_str());
}

}  // namespace

namespace extensions {

StartupHelper::StartupHelper() : pack_job_succeeded_(false) {}

void StartupHelper::OnPackSuccess(const FilePath& crx_path,
                                  const FilePath& output_private_key_path) {
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
  FilePath src_dir = cmd_line.GetSwitchValuePath(switches::kPackExtension);
  FilePath private_key_path;
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
  AppInstallHelper();
  virtual ~AppInstallHelper();
  bool success() { return success_; }
  const std::string& error() { return error_; }

  WebstoreStandaloneInstaller::Callback Callback();
  void OnAppInstallComplete(bool success, const std::string& error);

 private:
  // These hold on to the result of the app install when it is complete.
  bool success_;
  std::string error_;
};

AppInstallHelper::AppInstallHelper() : success_(false) {}

AppInstallHelper::~AppInstallHelper() {}

WebstoreStandaloneInstaller::Callback AppInstallHelper::Callback() {
  return base::Bind(&AppInstallHelper::OnAppInstallComplete,
                    base::Unretained(this));
}
void AppInstallHelper::OnAppInstallComplete(bool success,
                                            const std::string& error) {
  success_ = success;
  error_= error;
  MessageLoop::current()->Quit();
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

  // TODO(asargent) - it would be nice not to need a WebContents just to
  // use the standalone installer. (crbug.com/149039)
  scoped_ptr<content::WebContents> web_contents(content::WebContents::Create(
      content::WebContents::CreateParams(profile)));

  AppInstallHelper helper;
  WebstoreStandaloneInstaller::Callback callback =
      base::Bind(&AppInstallHelper::OnAppInstallComplete,
                 base::Unretained(&helper));
  scoped_refptr<WebstoreStandaloneInstaller> installer(
      new WebstoreStandaloneInstaller(
          web_contents.get(),
          id,
          WebstoreStandaloneInstaller::DO_NOT_REQUIRE_VERIFIED_SITE,
          WebstoreStandaloneInstaller::STANDARD_PROMPT,
          GURL(),
          callback));
  installer->set_skip_post_install_ui(true);
  installer->BeginInstall();

  MessageLoop::current()->Run();
  if (!helper.success())
    LOG(ERROR) << "InstallFromWebstore failed with error: " << helper.error();
  return helper.success();
}

StartupHelper::~StartupHelper() {
  if (pack_job_.get())
    pack_job_->ClearClient();
}

}  // namespace extensions
