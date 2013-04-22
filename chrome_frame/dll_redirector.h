// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_DLL_REDIRECTOR_H_
#define CHROME_FRAME_DLL_REDIRECTOR_H_

#include <ObjBase.h>
#include <windows.h>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/shared_memory.h"

// Forward
namespace ATL {
class CSecurityAttributes;
}

namespace base {
class Version;
}

// A singleton class that provides a facility to register the version of the
// current module as the only version that should be loaded system-wide. If
// this module is not the first instance loaded in the system, then the version
// that loaded first will be delegated to. This makes a few assumptions:
//  1) That different versions of the module this code is in reside in
//     neighbouring versioned directories, e.g.
//       C:\foo\bar\1.2.3.4\my_module.dll
//       C:\foo\bar\1.2.3.5\my_module.dll
//  2) That the instance of this class will outlive the module that may be
//     delegated to. That is to say, that this class only guarantees that the
//     module is loaded as long as this instance is active.
//  3) The module this is compiled into is built with version info.
class DllRedirector {
 public:
  // Returns the singleton instance.
  static DllRedirector* GetInstance();

  virtual ~DllRedirector();

  // Attempts to register this Chrome Frame version as the first loaded version
  // on the system. If this succeeds, return true. If it fails, it returns
  // false meaning that there is another version already loaded somewhere and
  // the caller should delegate to that version instead.
  bool DllRedirector::RegisterAsFirstCFModule();

  // Unregisters the well known window class if we registered it earlier.
  // This is intended to be called from DllMain under PROCESS_DETACH.
  void DllRedirector::UnregisterAsFirstCFModule();

  // Helper function to return the DllGetClassObject function pointer from
  // the given module. This function will return NULL unless
  // RegisterAsFirstCFModule has been called first and returned false
  // indicating that another module was first in.
  //
  // On success, the return value is non-null and the first-in module will have
  // had its reference count incremented.
  LPFNGETCLASSOBJECT GetDllGetClassObjectPtr();

 protected:
  DllRedirector();
  friend struct DefaultSingletonTraits<DllRedirector>;

  // Constructor used for tests.
  explicit DllRedirector(const char* shared_memory_name);

  // Returns an HMODULE to the version of the module that should be loaded.
  virtual HMODULE GetFirstModule();

  // Returns the version of the current module or NULL if none can be found.
  // The caller must free the returned version.
  virtual base::Version* GetCurrentModuleVersion();

  // Attempt to load the specified version dll. Finds it by walking up one
  // directory from our current module's location, then appending the newly
  // found version number. The Version class in base will have ensured that we
  // actually have a valid version and not e.g. ..\..\..\..\MyEvilFolder\.
  virtual HMODULE LoadVersionedModule(base::Version* version);

  // Builds the necessary SECURITY_ATTRIBUTES to allow low integrity access
  // to an object. Returns true on success, false otherwise.
  virtual bool BuildSecurityAttributesForLock(
      ATL::CSecurityAttributes* sec_attr);

  // Attempts to change the permissions on the given file mapping to read only.
  // Returns true on success, false otherwise.
  virtual bool SetFileMappingToReadOnly(base::SharedMemoryHandle mapping);

  // Shared memory segment that contains the version beacon.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // The current version of the DLL to be loaded.
  scoped_ptr<base::Version> dll_version_;

  // The handle to the first version of this module that was loaded. This
  // may refer to the current module, or another version of the same module
  // that we go and load.
  HMODULE first_module_handle_;

  // Used for tests to override the name of the shared memory segment.
  std::string shared_memory_name_;

  friend class ModuleUtilsTest;

  DISALLOW_COPY_AND_ASSIGN(DllRedirector);
};

#endif  // CHROME_FRAME_DLL_REDIRECTOR_H_
