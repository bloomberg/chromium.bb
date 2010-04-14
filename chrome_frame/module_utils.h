// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_MODULE_UTILS_H_
#define CHROME_FRAME_MODULE_UTILS_H_

#include <ObjBase.h>
#include <windows.h>

#include <map>

// A helper class that will find the named loaded module in the current
// process with the lowest version, increment its ref count and return
// a pointer to its DllGetClassObject() function if it exports one. If
// the oldest named module is the current module, then this class does nothing
// (does not muck with module ref count) and calls to
// get_dll_get_class_object_ptr() will return NULL.
class DllRedirector {
 public:
  typedef std::map<std::wstring, HMODULE> PathToHModuleMap;

  DllRedirector();
  ~DllRedirector();

  // Must call this before calling get_dll_get_class_object_ptr(). On first call
  // this performs the work of scanning the loaded modules for an old version
  // to delegate to. Not thread safe.
  void EnsureInitialized(const wchar_t* module_name, REFCLSID clsid);

  LPFNGETCLASSOBJECT get_dll_get_class_object_ptr() const;

 private:

  // Returns the pointer to the named loaded module's DllGetClassObject export
  // or NULL if either the pointer could not be found or if the pointer would
  // point into the current module.
  // Sets module_handle_ and increments the modules reference count.
  //
  // For sanity's sake, the module must return a non-null class factory for
  // the given class id.
  LPFNGETCLASSOBJECT GetDllGetClassObjectFromModuleName(
      const wchar_t* module_name, REFCLSID clsid);

  // Returns a handle in |module_handle| to the loaded module called
  // |module_name| in the current process. If there are multiple modules with
  // the same name, it returns the module with the oldest version number in its
  // VERSIONINFO block. The version string is expected to be of a form that
  // base::Version can parse.
  //
  // For sanity's sake, when there are multiple instances of the module,
  // |product_short_name|, if non-NULL, must match the module's
  // ProductShortName value
  //
  // Returns true if a named module with the given ProductShortName can be
  // found, returns false otherwise. Can return the current module handle.
  bool GetOldestNamedModuleHandle(const std::wstring& module_name,
                                  REFCLSID clsid,
                                  HMODULE* module_handle);

  // Given a PathToBaseAddressMap, iterates over the module images whose paths
  // are the keys and returns the handle to the module with the lowest
  // version number in its VERSIONINFO block whose DllGetClassObject returns a
  // class factory for the given CLSID.
  HMODULE GetHandleOfOldestModule(const PathToHModuleMap& map, REFCLSID clsid);

 private:
  // Helper function to return the DllGetClassObject function pointer from
  // the given module. On success, the return value is non-null and module
  // will have had its reference count incremented.
  LPFNGETCLASSOBJECT GetDllGetClassObjectPtr(HMODULE module);

  HMODULE module_handle_;
  LPFNGETCLASSOBJECT dcgo_ptr_;
  bool initialized_;

  friend class ModuleUtilsTest;
};

#endif  // CHROME_FRAME_MODULE_UTILS_H_
