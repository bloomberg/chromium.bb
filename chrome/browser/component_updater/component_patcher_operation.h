// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_H_

#include <string>
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "chrome/browser/component_updater/component_unpacker.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace component_updater {

class ComponentInstaller;
class ComponentPatcher;

class DeltaUpdateOp {
 public:

  DeltaUpdateOp();
  virtual ~DeltaUpdateOp();

  // Parses, runs, and verifies the operation, returning an error code if an
  // error is encountered, and DELTA_OK otherwise. In case of errors,
  // extended error information can be returned in the |error| parameter.
  ComponentUnpacker::Error Run(base::DictionaryValue* command_args,
                               const base::FilePath& input_dir,
                               const base::FilePath& unpack_dir,
                               ComponentPatcher* patcher,
                               ComponentInstaller* installer,
                               int* error);

 protected:
  std::string output_sha256_;
  base::FilePath output_abs_path_;

 private:
  ComponentUnpacker::Error CheckHash();

  // Subclasses must override DoParseArguments to parse operation-specific
  // arguments. DoParseArguments returns DELTA_OK on success; any other code
  // represents failure.
  virtual ComponentUnpacker::Error DoParseArguments(
      base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      ComponentInstaller* installer) = 0;

  // Subclasses must override DoRun to actually perform the patching operation.
  // DoRun returns DELTA_OK on success; any other code represents failure.
  // Additional error information can be returned in the |error| parameter.
  virtual ComponentUnpacker::Error DoRun(ComponentPatcher* patcher,
                                         int* error) = 0;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOp);
};

// A 'copy' operation takes a file currently residing on the disk and moves it
// into the unpacking directory: this represents "no change" in the file being
// installed.
class DeltaUpdateOpCopy : public DeltaUpdateOp {
 public:
  DeltaUpdateOpCopy();

 private:
  // Overrides of DeltaUpdateOp.
  virtual ComponentUnpacker::Error DoParseArguments(
      base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      ComponentInstaller* installer) OVERRIDE;

  virtual ComponentUnpacker::Error DoRun(ComponentPatcher* patcher,
                                         int* error) OVERRIDE;

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
  // Overrides of DeltaUpdateOp.
  virtual ComponentUnpacker::Error DoParseArguments(
      base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      ComponentInstaller* installer) OVERRIDE;

  virtual ComponentUnpacker::Error DoRun(ComponentPatcher* patcher,
                                         int* error) OVERRIDE;

  base::FilePath patch_abs_path_;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOpCreate);
};

// A 'bsdiff' operation takes an existing file on disk, and a bsdiff-
// format patch file provided in the delta update package, and runs bsdiff
// to construct an output file in the unpacking directory.
class DeltaUpdateOpPatchBsdiff : public DeltaUpdateOp {
 public:
  DeltaUpdateOpPatchBsdiff();

 private:
  // Overrides of DeltaUpdateOp.
  virtual ComponentUnpacker::Error DoParseArguments(
      base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      ComponentInstaller* installer) OVERRIDE;

  virtual ComponentUnpacker::Error DoRun(ComponentPatcher* patcher,
                                         int* error) OVERRIDE;

  base::FilePath patch_abs_path_;
  base::FilePath input_abs_path_;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOpPatchBsdiff);
};

// A 'courgette' operation takes an existing file on disk, and a Courgette-
// format patch file provided in the delta update package, and runs Courgette
// to construct an output file in the unpacking directory.
class DeltaUpdateOpPatchCourgette : public DeltaUpdateOp {
 public:
  DeltaUpdateOpPatchCourgette();

 private:
  // Overrides of DeltaUpdateOp.
  virtual ComponentUnpacker::Error DoParseArguments(
      base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      ComponentInstaller* installer) OVERRIDE;

  virtual ComponentUnpacker::Error DoRun(ComponentPatcher* patcher,
                                         int* error) OVERRIDE;

  base::FilePath patch_abs_path_;
  base::FilePath input_abs_path_;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOpPatchCourgette);
};

// Factory function to create DeltaUpdateOp instances.
DeltaUpdateOp* CreateDeltaUpdateOp(base::DictionaryValue* command);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_H_
