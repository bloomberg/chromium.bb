// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/component_updater/component_patcher.h"
#include "chrome/browser/component_updater/component_unpacker.h"
#include "content/public/browser/utility_process_host_client.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace component_updater {

class ComponentInstaller;

class DeltaUpdateOp : public base::RefCountedThreadSafe<DeltaUpdateOp> {
 public:
  DeltaUpdateOp();

  // Parses, runs, and verifies the operation. Calls |callback| with the
  // result of the operation. The callback is called using |task_runner|.
  void Run(const base::DictionaryValue* command_args,
           const base::FilePath& input_dir,
           const base::FilePath& unpack_dir,
           ComponentInstaller* installer,
           bool in_process,
           const ComponentUnpacker::Callback& callback,
           scoped_refptr<base::SequencedTaskRunner> task_runner);

 protected:
  virtual ~DeltaUpdateOp();

  bool InProcess();

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  std::string output_sha256_;
  base::FilePath output_abs_path_;

 private:
  friend class base::RefCountedThreadSafe<DeltaUpdateOp>;

  ComponentUnpacker::Error CheckHash();

  // Subclasses must override DoParseArguments to parse operation-specific
  // arguments. DoParseArguments returns DELTA_OK on success; any other code
  // represents failure.
  virtual ComponentUnpacker::Error DoParseArguments(
      const base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      ComponentInstaller* installer) = 0;

  // Subclasses must override DoRun to actually perform the patching operation.
  // They must call the provided callback when they have completed their
  // operations. In practice, the provided callback is always for "DoneRunning".
  virtual void DoRun(const ComponentUnpacker::Callback& callback) = 0;

  // Callback given to subclasses for when they complete their operation.
  // Validates the output, and posts a task to the patching operation's
  // callback.
  void DoneRunning(ComponentUnpacker::Error error, int extended_error);

  bool in_process_;
  ComponentUnpacker::Callback callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOp);
};

// A 'copy' operation takes a file currently residing on the disk and moves it
// into the unpacking directory: this represents "no change" in the file being
// installed.
class DeltaUpdateOpCopy : public DeltaUpdateOp {
 public:
  DeltaUpdateOpCopy();

 private:
  virtual ~DeltaUpdateOpCopy();

  // Overrides of DeltaUpdateOp.
  virtual ComponentUnpacker::Error DoParseArguments(
      const base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      ComponentInstaller* installer) OVERRIDE;

  virtual void DoRun(const ComponentUnpacker::Callback& callback) OVERRIDE;

  base::FilePath input_abs_path_;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOpCopy);
};

// A 'create' operation takes a full file that was sent in the delta update
// archive and moves it into the unpacking directory: this represents the
// addition of a new file, or a file so different that no bandwidth could be
// saved by transmitting a differential update.
class DeltaUpdateOpCreate : public DeltaUpdateOp {
 public:
  DeltaUpdateOpCreate();

 private:
  virtual ~DeltaUpdateOpCreate();

  // Overrides of DeltaUpdateOp.
  virtual ComponentUnpacker::Error DoParseArguments(
      const base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      ComponentInstaller* installer) OVERRIDE;

  virtual void DoRun(const ComponentUnpacker::Callback& callback) OVERRIDE;

  base::FilePath patch_abs_path_;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOpCreate);
};

class DeltaUpdateOpPatchStrategy {
 public:
  virtual ~DeltaUpdateOpPatchStrategy();

  // Returns an integer to add to error codes to disambiguate their source.
  virtual int GetErrorOffset() const = 0;

  // Returns the "error code" that is expected in the successful install case.
  virtual int GetSuccessCode() const = 0;

  // Returns an IPC message that will start patching if it is sent to a
  // UtilityProcessClient.
  virtual scoped_ptr<IPC::Message> GetPatchMessage(
      base::FilePath input_abs_path,
      base::FilePath patch_abs_path,
      base::FilePath output_abs_path) = 0;

  // Does the actual patching operation, and returns an error code.
  virtual int Patch(base::FilePath input_abs_path,
                    base::FilePath patch_abs_path,
                    base::FilePath output_abs_path) = 0;
};

class DeltaUpdateOpPatch;

class DeltaUpdateOpPatchHost : public content::UtilityProcessHostClient {
 public:
  DeltaUpdateOpPatchHost(scoped_refptr<DeltaUpdateOpPatch> patcher,
                         scoped_refptr<base::SequencedTaskRunner> task_runner);

  void StartProcess(scoped_ptr<IPC::Message> message);

 private:
  virtual ~DeltaUpdateOpPatchHost();

  void OnPatchSucceeded();

  void OnPatchFailed(int error_code);

  // Overrides of content::UtilityProcessHostClient.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  virtual void OnProcessCrashed(int exit_code) OVERRIDE;

  scoped_refptr<DeltaUpdateOpPatch> patcher_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

// Both 'bsdiff' and 'courgette' operations take an existing file on disk,
// and a bsdiff- or Courgette-format patch file provided in the delta update
// package, and run bsdiff or Courgette to construct an output file in the
// unpacking directory.
class DeltaUpdateOpPatch : public DeltaUpdateOp {
 public:
  explicit DeltaUpdateOpPatch(scoped_ptr<DeltaUpdateOpPatchStrategy> strategy);

  void DonePatching(ComponentUnpacker::Error error, int error_code);

 private:
  virtual ~DeltaUpdateOpPatch();

  // Overrides of DeltaUpdateOp.
  virtual ComponentUnpacker::Error DoParseArguments(
      const base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      ComponentInstaller* installer) OVERRIDE;

  virtual void DoRun(const ComponentUnpacker::Callback& callback) OVERRIDE;

  ComponentUnpacker::Callback callback_;
  base::FilePath patch_abs_path_;
  base::FilePath input_abs_path_;
  scoped_ptr<DeltaUpdateOpPatchStrategy> strategy_;
  scoped_refptr<DeltaUpdateOpPatchHost> host_;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOpPatch);
};

// Factory functions to create DeltaUpdateOp instances.
DeltaUpdateOp* CreateDeltaUpdateOp(const base::DictionaryValue& command);

DeltaUpdateOp* CreateDeltaUpdateOp(const std::string& operation);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_H_
