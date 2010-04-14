// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test requires loading a set of DLLs from the chrome_frame\test\data
// directory into the process and then inspecting them. As such, it is
// part of chrome_frame_tests.exe and not chrome_frame_unittests.exe which
// needs to run as a standalone test. No test is an island except for
// chrome_frame_unittests.exe.

#include "chrome_frame/module_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/logging.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome_frame/test_utils.h"

#include "chrome_tab.h"  // NOLINT

class ModuleUtilsTest : public testing::Test {
 protected:
  // Constructor
  ModuleUtilsTest() {}

  // Returns the full path to the test DLL given a name.
  virtual bool GetDllPath(const std::wstring& dll_name, std::wstring* path) {
    if (!path) {
      return false;
    }

    FilePath test_path;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &test_path)) {
      return false;
    }

    test_path = test_path.Append(L"chrome_frame")
                         .Append(L"test")
                         .Append(L"data")
                         .Append(L"test_dlls")
                         .Append(FilePath(dll_name));

    *path = test_path.value();
    return true;
  }

  // Loads the CF Dll and returns its path in |cf_dll_path|.
  virtual bool LoadChromeFrameDll(std::wstring* cf_dll_path) {
    DCHECK(cf_dll_path);
    // Look for the CF dll in both the current directory and in servers.
    FilePath dll_path = ScopedChromeFrameRegistrar::GetChromeFrameBuildPath();

    bool success = false;
    if (!dll_path.empty()) {
      cf_dll_path_ = dll_path.value();
      HMODULE handle = LoadLibrary(cf_dll_path_.c_str());
      if (handle) {
        hmodule_map_[cf_dll_path_] = handle;
        *cf_dll_path = cf_dll_path_;
        success = true;
      } else {
        LOG(ERROR) << "Failed to load test dll: " << dll_path.value();
      }
    }

    return success;
  }

  virtual bool LoadTestDll(const std::wstring& dll_name) {
    bool success = false;
    std::wstring dll_path;
    if (GetDllPath(dll_name, &dll_path)) {
      HMODULE handle = LoadLibrary(dll_path.c_str());
      if (handle) {
        hmodule_map_[dll_name] = handle;
        success = true;
      } else {
        LOG(ERROR) << "Failed to load test dll: " << dll_name;
      }
    } else {
      LOG(ERROR) << "Failed to get dll path for " << dll_name;
    }
    return success;
  }

  // Unload any DLLs we have loaded and make sure they stay unloaded.
  virtual void TearDown() {
    DllRedirector::PathToHModuleMap::const_iterator iter(hmodule_map_.begin());
    for (; iter != hmodule_map_.end(); ++iter) {
      FreeLibrary(iter->second);
    }

    // Check that the modules were actually unloaded (i.e. we had no dangling
    // references). Do this after freeing all modules since they can have
    // references to each other.
    for (iter = hmodule_map_.begin(); iter != hmodule_map_.end(); ++iter) {
      // The CF module gets pinned, so don't check that that is unloaded.
      if (iter->first != cf_dll_path_) {
        HMODULE temp_handle;
        ASSERT_FALSE(GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                       reinterpret_cast<LPCTSTR>(iter->second),
                                       &temp_handle));
      }
    }

    hmodule_map_.clear();
  }

  DllRedirector::PathToHModuleMap hmodule_map_;
  std::wstring cf_dll_path_;
};

// Tests that if we load a few versions of the same module that all export
// DllGetClassObject, that we correctly a) find a DllGetClassObject function
// pointer and b) find it in the right module.
TEST_F(ModuleUtilsTest, BasicTest) {
  ASSERT_TRUE(LoadTestDll(L"3\\TestDll.dll"));
  ASSERT_TRUE(LoadTestDll(L"2\\TestDll.dll"));
  ASSERT_TRUE(LoadTestDll(L"1\\TestDll.dll"));

  DllRedirector redir;
  redir.EnsureInitialized(L"TestDll.dll", CLSID_ChromeActiveDocument);

  LPFNGETCLASSOBJECT found_ptr = redir.get_dll_get_class_object_ptr();
  EXPECT_TRUE(found_ptr != NULL);

  LPFNGETCLASSOBJECT direct_ptr = reinterpret_cast<LPFNGETCLASSOBJECT>(
      GetProcAddress(hmodule_map_[L"1\\TestDll.dll"],
                     "DllGetClassObject"));
  EXPECT_TRUE(direct_ptr != NULL);

  EXPECT_EQ(found_ptr, direct_ptr);
}

// Tests that a DLL that does not return a class factory for a Chrome Frame
// guid even though it has a lower version string.
TEST_F(ModuleUtilsTest, NoCFDllTest) {
  ASSERT_TRUE(LoadTestDll(L"1\\TestDll.dll"));
  ASSERT_TRUE(LoadTestDll(L"TestDllNoCF\\TestDll.dll"));

  DllRedirector redir;
  redir.EnsureInitialized(L"TestDll.dll", CLSID_ChromeActiveDocument);

  LPFNGETCLASSOBJECT found_ptr = redir.get_dll_get_class_object_ptr();
  EXPECT_TRUE(found_ptr != NULL);

  LPFNGETCLASSOBJECT direct_ptr =
      reinterpret_cast<LPFNGETCLASSOBJECT>(
          GetProcAddress(hmodule_map_[L"1\\TestDll.dll"],
                         "DllGetClassObject"));
  EXPECT_TRUE(direct_ptr != NULL);

  EXPECT_EQ(found_ptr, direct_ptr);
}

// Tests that this works with the actual CF dll.
TEST_F(ModuleUtilsTest, ChromeFrameDllTest) {
  ASSERT_TRUE(LoadTestDll(L"DummyCF\\npchrome_frame.dll"));
  std::wstring cf_dll_path;
  ASSERT_TRUE(LoadChromeFrameDll(&cf_dll_path));
  ASSERT_TRUE(!cf_dll_path.empty());

  DllRedirector redir;
  redir.EnsureInitialized(L"npchrome_frame.dll", CLSID_ChromeActiveDocument);

  LPFNGETCLASSOBJECT found_ptr = redir.get_dll_get_class_object_ptr();
  EXPECT_TRUE(found_ptr != NULL);

  LPFNGETCLASSOBJECT direct_ptr = reinterpret_cast<LPFNGETCLASSOBJECT>(
      GetProcAddress(hmodule_map_[L"DummyCF\\npchrome_frame.dll"],
                     "DllGetClassObject"));
  EXPECT_TRUE(direct_ptr != NULL);

  EXPECT_EQ(found_ptr, direct_ptr);

  // Now try asking for a ChromeActiveDocument using the non-dummy CF DLL
  // handle and make sure that the delegation to the dummy module happens
  // correctly. Use the bare guid to keep dependencies simple
  const wchar_t kClsidChromeActiveDocument[] =
      L"{3e1d0e7f-f5e3-44cc-aa6a-c0a637619ab8}";

  LPFNGETCLASSOBJECT cf_ptr = reinterpret_cast<LPFNGETCLASSOBJECT>(
      GetProcAddress(hmodule_map_[cf_dll_path],
                     "DllGetClassObject"));
  EXPECT_TRUE(cf_ptr != NULL);

  CLSID cf_clsid;
  HRESULT hr = CLSIDFromString(kClsidChromeActiveDocument, &cf_clsid);
  EXPECT_HRESULT_SUCCEEDED(hr);

  CComPtr<IClassFactory> class_factory;
  DWORD result = cf_ptr(cf_clsid, IID_IClassFactory,
                        reinterpret_cast<void**>(&class_factory));

  EXPECT_EQ(S_OK, result);
}
