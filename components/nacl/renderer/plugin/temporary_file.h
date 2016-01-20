// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_TEMPORARY_FILE_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_TEMPORARY_FILE_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

#include "ppapi/c/private/pp_file_handle.h"

namespace plugin {

class Plugin;

// Translation creates two temporary files.  The first temporary file holds
// the object file created by llc.  The second holds the nexe produced by
// the linker.  Both of these temporary files are used to both write and
// read according to the following matrix:
//
// PnaclCoordinator::obj_file_:
//     written by: llc     (passed in explicitly through SRPC)
//     read by:    ld      (returned via lookup service from SRPC)
// PnaclCoordinator::nexe_file_:
//     written by: ld      (passed in explicitly through SRPC)
//     read by:    sel_ldr (passed in explicitly to command channel)
//

// TempFile represents a file used as a temporary between stages in
// translation.  It is automatically deleted when all handles are closed
// (or earlier -- immediately unlinked on POSIX systems).  The file is only
// opened, once, but because both reading and writing are necessary (and in
// different processes), the user should reset / seek back to the beginning
// of the file between sessions.
class TempFile {
 public:
  // Create a TempFile.
  TempFile(Plugin* plugin, PP_FileHandle handle);
  ~TempFile();

  bool IsValid() { return file_handle_.IsValid(); }
  // Resets file position of the handle, for reuse.
  bool Reset();

  // Returns the current size of this file.
  int64_t GetLength();

  // Returns a handle to the file, transferring ownership of it.
  PP_FileHandle TakeFileHandle();

  // Returns a handle to the file, without transferring ownership of it.
  // This handle remains valid until the TempFile object is destroyed or
  // TakeFileHandle() is called.
  PP_FileHandle GetFileHandle();

 private:
  Plugin* plugin_;
  base::File file_handle_;

  DISALLOW_COPY_AND_ASSIGN(TempFile);
};

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_TEMPORARY_FILE_H_
