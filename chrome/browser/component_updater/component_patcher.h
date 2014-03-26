// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Component updates can be either differential updates or full updates.
// Full updates come in CRX format; differential updates come in CRX-style
// archives, but have a different magic number. They contain "commands.json", a
// list of commands for the patcher to follow. The patcher uses these commands,
// the other files in the archive, and the files from the existing installation
// of the component to create the contents of a full update, which is then
// installed normally.
// Component updates are specified by the 'codebasediff' attribute of an
// updatecheck response:
//   <updatecheck codebase="http://example.com/extension_1.2.3.4.crx"
//                hash="12345" size="9854" status="ok" version="1.2.3.4"
//                prodversionmin="2.0.143.0"
//                codebasediff="http://example.com/diff_1.2.3.4.crx"
//                hashdiff="123" sizediff="101"
//                fp="1.123" />
// The component updater will attempt a differential update if it is available
// and allowed to, and fall back to a full update if it fails.
//
// After installation (diff or full), the component updater records "fp", the
// fingerprint of the installed files, to later identify the existing files to
// the server so that a proper differential update can be provided next cycle.


#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "chrome/browser/component_updater/component_unpacker.h"

namespace base {
class FilePath;
}

namespace component_updater {

class ComponentInstaller;

// Applies a delta patch to a single file. Specifically, creates a file at
// |output_file| using |input_file| patched according to the algorithm
// specified by |patch_type| using |patch_file|. Sets the value of error to
// the error code of the failing patch operation, if there is such a failure.
class ComponentPatcher {
 public:
  // The type of a patch file.
  enum PatchType {
    kPatchTypeUnknown,
    kPatchTypeCourgette,
    kPatchTypeBsdiff,
  };

  virtual ComponentUnpacker::Error Patch(PatchType patch_type,
                                         const base::FilePath& input_file,
                                         const base::FilePath& patch_file,
                                         const base::FilePath& output_file,
                                         int* error) = 0;
  virtual ~ComponentPatcher() {}
};

class ComponentPatcherCrossPlatform : public ComponentPatcher {
 public:
  ComponentPatcherCrossPlatform();
  virtual ComponentUnpacker::Error Patch(PatchType patch_type,
                                         const base::FilePath& input_file,
                                         const base::FilePath& patch_file,
                                         const base::FilePath& output_file,
                                         int* error) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(ComponentPatcherCrossPlatform);
};

// This function takes an unpacked differential CRX (|input_dir|) and a
// component installer, and creates a new (non-differential) unpacked CRX, which
// is then installed normally.
// The non-differential files are written into the |unpack_dir| directory.
// When finished, calls the callback, passing error codes if any errors were
// encountered.
void DifferentialUpdatePatch(
    const base::FilePath& input_dir,
    const base::FilePath& unpack_dir,
    ComponentPatcher* component_patcher,
    ComponentInstaller* installer,
    base::Callback<void(ComponentUnpacker::Error, int)> callback);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_H_
