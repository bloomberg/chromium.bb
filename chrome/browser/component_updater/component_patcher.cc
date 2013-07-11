// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_patcher.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/values.h"
#include "chrome/browser/component_updater/component_patcher_operation.h"
#include "chrome/browser/component_updater/component_updater_service.h"

namespace {

// Deserialize the commands file (present in delta update packages). The top
// level must be a list.
base::ListValue* ReadCommands(const base::FilePath& unpack_path) {
  const base::FilePath commands =
      unpack_path.Append(FILE_PATH_LITERAL("commands.json"));
  if (!base::PathExists(commands))
    return NULL;

  JSONFileValueSerializer serializer(commands);
  scoped_ptr<base::Value> root(serializer.Deserialize(NULL, NULL));

  return (root.get() && root->IsType(base::Value::TYPE_LIST)) ?
      static_cast<base::ListValue*>(root.release()) : NULL;
}

}  // namespace


// The patching support is not cross-platform at the moment.
ComponentPatcherCrossPlatform::ComponentPatcherCrossPlatform() {}

ComponentUnpacker::Error ComponentPatcherCrossPlatform::Patch(
    PatchType patch_type,
    const base::FilePath& input_file,
    const base::FilePath& patch_file,
    const base::FilePath& output_file,
    int* error) {
  return ComponentUnpacker::kDeltaUnsupportedCommand;
}


// Takes the contents of a differential component update in input_dir
// and produces the contents of a full component update in unpack_dir
// using input_abs_path_ files that the installer knows about.
ComponentUnpacker::Error DifferentialUpdatePatch(
    const base::FilePath& input_dir,
    const base::FilePath& unpack_dir,
    ComponentPatcher* patcher,
    ComponentInstaller* installer,
    int* error) {
  *error = 0;
  scoped_ptr<base::ListValue> commands(ReadCommands(input_dir));
  if (!commands.get())
    return ComponentUnpacker::kDeltaBadCommands;

  for (base::ValueVector::const_iterator command = commands->begin(),
      end = commands->end(); command != end; command++) {
    if (!(*command)->IsType(base::Value::TYPE_DICTIONARY))
      return ComponentUnpacker::kDeltaBadCommands;
    base::DictionaryValue* command_args =
        static_cast<base::DictionaryValue*>(*command);
    scoped_ptr<DeltaUpdateOp> operation(CreateDeltaUpdateOp(command_args));
    if (!operation)
      return ComponentUnpacker::kDeltaUnsupportedCommand;

    ComponentUnpacker::Error result = operation->Run(
        command_args, input_dir, unpack_dir, patcher, installer, error);
    if (result != ComponentUnpacker::kNone)
      return result;
  }

  return ComponentUnpacker::kNone;
}

