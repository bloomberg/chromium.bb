// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/startup_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/extension.h"

using content::BrowserThread;

namespace extensions {

namespace {

void PrintPackExtensionMessage(const std::string& message) {
  VLOG(1) << message;
}

}  // namespace

StartupHelper::StartupHelper() : pack_job_succeeded_(false) {
  ExtensionsClient::Set(ChromeExtensionsClient::GetInstance());
}

void StartupHelper::OnPackSuccess(
    const base::FilePath& crx_path,
    const base::FilePath& output_private_key_path) {
  pack_job_succeeded_ = true;
  PrintPackExtensionMessage(
      base::UTF16ToUTF8(
          PackExtensionJob::StandardSuccessMessage(crx_path,
                                                   output_private_key_path)));
}

void StartupHelper::OnPackFailure(const std::string& error_message,
                                  ExtensionCreator::ErrorType type) {
  PrintPackExtensionMessage(error_message);
}

bool StartupHelper::PackExtension(const base::CommandLine& cmd_line) {
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

namespace {

class ValidateCrxHelper : public SandboxedUnpackerClient {
 public:
  ValidateCrxHelper(const CRXFileInfo& file,
                    const base::FilePath& temp_dir,
                    base::RunLoop* run_loop)
      : crx_file_(file),
        temp_dir_(temp_dir),
        run_loop_(run_loop),
        finished_(false),
        success_(false) {}

  bool finished() { return finished_; }
  bool success() { return success_; }
  const base::string16& error() { return error_; }

  void Start() {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&ValidateCrxHelper::StartOnFileThread,
                                       this));
  }

 protected:
  ~ValidateCrxHelper() override {}

  void OnUnpackSuccess(const base::FilePath& temp_dir,
                       const base::FilePath& extension_root,
                       const base::DictionaryValue* original_manifest,
                       const Extension* extension,
                       const SkBitmap& install_icon) override {
    finished_ = true;
    success_ = true;
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&ValidateCrxHelper::FinishOnUIThread,
                                       this));
  }

  void OnUnpackFailure(const CrxInstallError& error) override {
    finished_ = true;
    success_ = false;
    error_ = error.message();
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&ValidateCrxHelper::FinishOnUIThread,
                                       this));
  }

  void FinishOnUIThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (run_loop_->running())
      run_loop_->Quit();
  }

  void StartOnFileThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner =
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE);

    scoped_refptr<SandboxedUnpacker> unpacker(new SandboxedUnpacker(
        Manifest::INTERNAL, 0, /* no special creation flags */
        temp_dir_, file_task_runner.get(), this));
    unpacker->StartWithCrx(crx_file_);
  }

  // The file being validated.
  const CRXFileInfo& crx_file_;

  // The temporary directory where the sandboxed unpacker will do work.
  const base::FilePath& temp_dir_;

  // Unowned pointer to a runloop, so our consumer can wait for us to finish.
  base::RunLoop* run_loop_;

  // Whether we're finished unpacking;
  bool finished_;

  // Whether the unpacking was successful.
  bool success_;

  // If the unpacking wasn't successful, this contains an error message.
  base::string16 error_;
};

}  // namespace

bool StartupHelper::ValidateCrx(const base::CommandLine& cmd_line,
                                std::string* error) {
  CHECK(error);
  base::FilePath path = cmd_line.GetSwitchValuePath(switches::kValidateCrx);
  if (path.empty()) {
    *error = base::StringPrintf("Empty path passed for %s",
                                switches::kValidateCrx);
    return false;
  }
  base::ScopedTempDir temp_dir;

  if (!temp_dir.CreateUniqueTempDir()) {
    *error = std::string("Failed to create temp dir");
    return false;
  }

  base::RunLoop run_loop;
  CRXFileInfo file(path);
  scoped_refptr<ValidateCrxHelper> helper(
      new ValidateCrxHelper(file, temp_dir.GetPath(), &run_loop));
  helper->Start();
  run_loop.Run();

  bool success = helper->success();
  if (!success)
    *error = base::UTF16ToUTF8(helper->error());
  return success;
}

StartupHelper::~StartupHelper() {
  if (pack_job_.get())
    pack_job_->ClearClient();
}

}  // namespace extensions
