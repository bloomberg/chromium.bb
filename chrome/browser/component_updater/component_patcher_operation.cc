// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_patcher_operation.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/component_updater/component_patcher.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "extensions/common/crx_file.h"
#include "ipc/ipc_message_macros.h"

using crypto::SecureHash;

namespace component_updater {

namespace {

const char kInput[] = "input";
const char kOp[] = "op";
const char kOutput[] = "output";
const char kPatch[] = "patch";
const char kSha256[] = "sha256";

// The integer offset disambiguates between overlapping error ranges.
const int kCourgetteErrorOffset = 300;
const int kBsdiffErrorOffset = 600;

class CourgetteTraits : public DeltaUpdateOpPatchStrategy {
 public:
  virtual int GetErrorOffset() const OVERRIDE;
  virtual int GetSuccessCode() const OVERRIDE;
  virtual scoped_ptr<IPC::Message> GetPatchMessage(
      base::FilePath input_abs_path,
      base::FilePath patch_abs_path,
      base::FilePath output_abs_path) OVERRIDE;
  virtual int Patch(base::FilePath input_abs_path,
                    base::FilePath patch_abs_path,
                    base::FilePath output_abs_path) OVERRIDE;
};

int CourgetteTraits::GetErrorOffset() const {
  return kCourgetteErrorOffset;
}

int CourgetteTraits::GetSuccessCode() const {
  return courgette::C_OK;
}

scoped_ptr<IPC::Message> CourgetteTraits::GetPatchMessage(
    base::FilePath input_abs_path,
    base::FilePath patch_abs_path,
    base::FilePath output_abs_path) {
  return scoped_ptr<IPC::Message>(
      new ChromeUtilityMsg_PatchFileCourgette(input_abs_path,
                                              patch_abs_path,
                                              output_abs_path));
}

int CourgetteTraits::Patch(base::FilePath input_abs_path,
                           base::FilePath patch_abs_path,
                           base::FilePath output_abs_path) {
  return courgette::ApplyEnsemblePatch(input_abs_path.value().c_str(),
                                       patch_abs_path.value().c_str(),
                                       output_abs_path.value().c_str());
}

class BsdiffTraits : public DeltaUpdateOpPatchStrategy {
 public:
  virtual int GetErrorOffset() const OVERRIDE;
  virtual int GetSuccessCode() const OVERRIDE;
  virtual scoped_ptr<IPC::Message> GetPatchMessage(
      base::FilePath input_abs_path,
      base::FilePath patch_abs_path,
      base::FilePath output_abs_path) OVERRIDE;
  virtual int Patch(base::FilePath input_abs_path,
                    base::FilePath patch_abs_path,
                    base::FilePath output_abs_path) OVERRIDE;
};

int BsdiffTraits::GetErrorOffset() const {
  return kBsdiffErrorOffset;
}

int BsdiffTraits::GetSuccessCode() const {
  return courgette::OK;
}

scoped_ptr<IPC::Message> BsdiffTraits::GetPatchMessage(
    base::FilePath input_abs_path,
    base::FilePath patch_abs_path,
    base::FilePath output_abs_path) {
  return scoped_ptr<IPC::Message>(
      new ChromeUtilityMsg_PatchFileBsdiff(input_abs_path,
                                           patch_abs_path,
                                           output_abs_path));
}

int BsdiffTraits::Patch(base::FilePath input_abs_path,
                        base::FilePath patch_abs_path,
                        base::FilePath output_abs_path) {
  return courgette::ApplyBinaryPatch(input_abs_path,
                                     patch_abs_path,
                                     output_abs_path);
}

}  // namespace

DeltaUpdateOpPatchStrategy::~DeltaUpdateOpPatchStrategy() {
}

DeltaUpdateOp* CreateDeltaUpdateOp(const std::string& operation) {
  if (operation == "copy") {
    return new DeltaUpdateOpCopy();
  } else if (operation == "create") {
    return new DeltaUpdateOpCreate();
  } else if (operation == "bsdiff") {
    scoped_ptr<DeltaUpdateOpPatchStrategy> strategy(new BsdiffTraits());
    return new DeltaUpdateOpPatch(strategy.Pass());
  } else if (operation == "courgette") {
    scoped_ptr<DeltaUpdateOpPatchStrategy> strategy(new CourgetteTraits());
    return new DeltaUpdateOpPatch(strategy.Pass());
  }
  return NULL;
}

DeltaUpdateOp* CreateDeltaUpdateOp(const base::DictionaryValue& command) {
  std::string operation;
  if (!command.GetString(kOp, &operation))
    return NULL;
  return CreateDeltaUpdateOp(operation);
}

DeltaUpdateOp::DeltaUpdateOp() : in_process_(false) {
}

DeltaUpdateOp::~DeltaUpdateOp() {
}

void DeltaUpdateOp::Run(const base::DictionaryValue* command_args,
                        const base::FilePath& input_dir,
                        const base::FilePath& unpack_dir,
                        ComponentInstaller* installer,
                        bool in_process,
                        const ComponentUnpacker::Callback& callback,
                        scoped_refptr<base::SequencedTaskRunner> task_runner) {
  callback_ = callback;
  in_process_ = in_process;
  task_runner_ = task_runner;
  std::string output_rel_path;
  if (!command_args->GetString(kOutput, &output_rel_path) ||
      !command_args->GetString(kSha256, &output_sha256_)) {
    DoneRunning(ComponentUnpacker::kDeltaBadCommands, 0);
    return;
  }

  output_abs_path_ =
      unpack_dir.Append(base::FilePath::FromUTF8Unsafe(output_rel_path));
  ComponentUnpacker::Error parse_result =
      DoParseArguments(command_args, input_dir, installer);
  if (parse_result != ComponentUnpacker::kNone) {
    DoneRunning(parse_result, 0);
    return;
  }

  const base::FilePath parent = output_abs_path_.DirName();
  if (!base::DirectoryExists(parent)) {
    if (!base::CreateDirectory(parent)) {
      DoneRunning(ComponentUnpacker::kIoError, 0);
      return;
    }
  }

  DoRun(base::Bind(&DeltaUpdateOp::DoneRunning,
                   scoped_refptr<DeltaUpdateOp>(this)));
}

void DeltaUpdateOp::DoneRunning(ComponentUnpacker::Error error,
                                int extended_error) {
  if (error == ComponentUnpacker::kNone)
    error = CheckHash();
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(callback_, error, extended_error));
  callback_.Reset();
}

// Uses the hash as a checksum to confirm that the file now residing in the
// output directory probably has the contents it should.
ComponentUnpacker::Error DeltaUpdateOp::CheckHash() {
  std::vector<uint8> expected_hash;
  if (!base::HexStringToBytes(output_sha256_, &expected_hash) ||
      expected_hash.size() != crypto::kSHA256Length)
    return ComponentUnpacker::kDeltaVerificationFailure;

  base::MemoryMappedFile output_file_mmapped;
  if (!output_file_mmapped.Initialize(output_abs_path_))
    return ComponentUnpacker::kDeltaVerificationFailure;

  uint8 actual_hash[crypto::kSHA256Length] = {0};
  const scoped_ptr<SecureHash> hasher(SecureHash::Create(SecureHash::SHA256));
  hasher->Update(output_file_mmapped.data(), output_file_mmapped.length());
  hasher->Finish(actual_hash, sizeof(actual_hash));
  if (memcmp(actual_hash, &expected_hash[0], sizeof(actual_hash)))
    return ComponentUnpacker::kDeltaVerificationFailure;

  return ComponentUnpacker::kNone;
}

bool DeltaUpdateOp::InProcess() {
  return in_process_;
}

scoped_refptr<base::SequencedTaskRunner> DeltaUpdateOp::GetTaskRunner() {
  return task_runner_;
}

DeltaUpdateOpCopy::DeltaUpdateOpCopy() {
}

DeltaUpdateOpCopy::~DeltaUpdateOpCopy() {
}

ComponentUnpacker::Error DeltaUpdateOpCopy::DoParseArguments(
    const base::DictionaryValue* command_args,
    const base::FilePath& input_dir,
    ComponentInstaller* installer) {
  std::string input_rel_path;
  if (!command_args->GetString(kInput, &input_rel_path))
    return ComponentUnpacker::kDeltaBadCommands;

  if (!installer->GetInstalledFile(input_rel_path, &input_abs_path_))
    return ComponentUnpacker::kDeltaMissingExistingFile;

  return ComponentUnpacker::kNone;
}

void DeltaUpdateOpCopy::DoRun(const ComponentUnpacker::Callback& callback) {
  if (!base::CopyFile(input_abs_path_, output_abs_path_))
    callback.Run(ComponentUnpacker::kDeltaOperationFailure, 0);
  else
    callback.Run(ComponentUnpacker::kNone, 0);
}

DeltaUpdateOpCreate::DeltaUpdateOpCreate() {
}

DeltaUpdateOpCreate::~DeltaUpdateOpCreate() {
}

ComponentUnpacker::Error DeltaUpdateOpCreate::DoParseArguments(
    const base::DictionaryValue* command_args,
    const base::FilePath& input_dir,
    ComponentInstaller* installer) {
  std::string patch_rel_path;
  if (!command_args->GetString(kPatch, &patch_rel_path))
    return ComponentUnpacker::kDeltaBadCommands;

  patch_abs_path_ =
      input_dir.Append(base::FilePath::FromUTF8Unsafe(patch_rel_path));

  return ComponentUnpacker::kNone;
}

void DeltaUpdateOpCreate::DoRun(const ComponentUnpacker::Callback& callback) {
  if (!base::Move(patch_abs_path_, output_abs_path_))
    callback.Run(ComponentUnpacker::kDeltaOperationFailure, 0);
  else
    callback.Run(ComponentUnpacker::kNone, 0);
}

DeltaUpdateOpPatchHost::DeltaUpdateOpPatchHost(
    scoped_refptr<DeltaUpdateOpPatch> patcher,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : patcher_(patcher), task_runner_(task_runner) {
}

DeltaUpdateOpPatchHost::~DeltaUpdateOpPatchHost() {
}

void DeltaUpdateOpPatchHost::StartProcess(scoped_ptr<IPC::Message> message) {
  // The DeltaUpdateOpPatchHost is not responsible for deleting the
  // UtilityProcessHost object.
  content::UtilityProcessHost* host = content::UtilityProcessHost::Create(
      this, base::MessageLoopProxy::current().get());
  host->DisableSandbox();
  host->Send(message.release());
}

bool DeltaUpdateOpPatchHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeltaUpdateOpPatchHost, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_PatchFile_Succeeded,
                        OnPatchSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_PatchFile_Failed,
                        OnPatchFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeltaUpdateOpPatchHost::OnPatchSucceeded() {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&DeltaUpdateOpPatch::DonePatching,
                                    patcher_,
                                    ComponentUnpacker::kNone,
                                    0));
  task_runner_ = NULL;
  patcher_ = NULL;
}

void DeltaUpdateOpPatchHost::OnPatchFailed(int error_code) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&DeltaUpdateOpPatch::DonePatching,
                                    patcher_,
                                    ComponentUnpacker::kDeltaOperationFailure,
                                    error_code));
  task_runner_ = NULL;
  patcher_ = NULL;
}

void DeltaUpdateOpPatchHost::OnProcessCrashed(int exit_code) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DeltaUpdateOpPatch::DonePatching,
                 patcher_,
                 ComponentUnpacker::kDeltaPatchProcessFailure,
                 exit_code));
  task_runner_ = NULL;
  patcher_ = NULL;
}

DeltaUpdateOpPatch::DeltaUpdateOpPatch(
    scoped_ptr<DeltaUpdateOpPatchStrategy> strategy) {
  strategy_ = strategy.Pass();
}

DeltaUpdateOpPatch::~DeltaUpdateOpPatch() {
}

ComponentUnpacker::Error DeltaUpdateOpPatch::DoParseArguments(
    const base::DictionaryValue* command_args,
    const base::FilePath& input_dir,
    ComponentInstaller* installer) {
  std::string patch_rel_path;
  std::string input_rel_path;
  if (!command_args->GetString(kPatch, &patch_rel_path) ||
      !command_args->GetString(kInput, &input_rel_path))
    return ComponentUnpacker::kDeltaBadCommands;

  if (!installer->GetInstalledFile(input_rel_path, &input_abs_path_))
    return ComponentUnpacker::kDeltaMissingExistingFile;

  patch_abs_path_ =
      input_dir.Append(base::FilePath::FromUTF8Unsafe(patch_rel_path));

  return ComponentUnpacker::kNone;
}

void DeltaUpdateOpPatch::DoRun(const ComponentUnpacker::Callback& callback) {
  callback_ = callback;
  if (!InProcess()) {
    host_ = new DeltaUpdateOpPatchHost(scoped_refptr<DeltaUpdateOpPatch>(this),
                                       GetTaskRunner());
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DeltaUpdateOpPatchHost::StartProcess,
                   host_,
                   base::Passed(strategy_->GetPatchMessage(input_abs_path_,
                                                           patch_abs_path_,
                                                           output_abs_path_))));
    return;
  }
  const int result = strategy_->Patch(input_abs_path_,
                                      patch_abs_path_,
                                      output_abs_path_);
  if (result == strategy_->GetSuccessCode())
    DonePatching(ComponentUnpacker::kNone, 0);
  else
    DonePatching(ComponentUnpacker::kDeltaOperationFailure, result);
}

void DeltaUpdateOpPatch::DonePatching(ComponentUnpacker::Error error,
                                      int error_code) {
  host_ = NULL;
  if (error != ComponentUnpacker::kNone) {
    error_code += strategy_->GetErrorOffset();
  }
  callback_.Run(error, error_code);
  // The callback is no longer needed - it is best to release it in case it
  // contains a reference to this object.
  callback_.Reset();
}

}  // namespace component_updater
