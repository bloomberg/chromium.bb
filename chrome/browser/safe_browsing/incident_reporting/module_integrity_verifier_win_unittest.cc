// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_verifier_win.h"

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/native_library.h"
#include "base/scoped_native_library.h"
#include "base/win/pe_image.h"
#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_unittest_util_win.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingModuleVerifierWinTest : public testing::Test {
 protected:
  void SetUpTestDllAndPEImages() {
    LoadModule();
    HMODULE mem_handle;
    GetMemModuleHandle(&mem_handle);
    mem_peimage_ptr_.reset(new base::win::PEImage(mem_handle));
    ASSERT_TRUE(mem_peimage_ptr_->VerifyMagic());

    LoadDLLAsFile();
    HMODULE disk_handle;
    GetDiskModuleHandle(&disk_handle);
    disk_peimage_ptr_.reset(new base::win::PEImageAsData(disk_handle));
    ASSERT_TRUE(disk_peimage_ptr_->VerifyMagic());
  }

  void LoadModule() {
    HMODULE mem_dll_handle =
        LoadNativeLibrary(base::FilePath(kTestDllNames[0]), NULL);
    ASSERT_NE(static_cast<HMODULE>(NULL), mem_dll_handle)
        << "GLE=" << GetLastError();
    mem_dll_handle_.Reset(mem_dll_handle);
    ASSERT_TRUE(mem_dll_handle_.is_valid());
  }

  void GetMemModuleHandle(HMODULE* mem_handle) {
    *mem_handle = GetModuleHandle(kTestDllNames[0]);
    ASSERT_NE(static_cast<HMODULE>(NULL), *mem_handle);
  }

  void LoadDLLAsFile() {
    // Use the module handle to find the it on disk, then load as a file.
    HMODULE module_handle;
    GetMemModuleHandle(&module_handle);

    WCHAR module_path[MAX_PATH] = {};
    DWORD length =
        GetModuleFileName(module_handle, module_path, arraysize(module_path));
    ASSERT_NE(arraysize(module_path), length);
    ASSERT_TRUE(disk_dll_handle_.Initialize(base::FilePath(module_path)));
  }

  void GetDiskModuleHandle(HMODULE* disk_handle) {
    *disk_handle =
        reinterpret_cast<HMODULE>(const_cast<uint8*>(disk_dll_handle_.data()));
  }

  // Edits the first byte of the single function exported by the test dll.
  void EditExport() {
    HMODULE mem_handle;
    GetMemModuleHandle(&mem_handle);
    uint8_t* export_addr =
        reinterpret_cast<uint8_t*>(GetProcAddress(mem_handle, kTestExportName));
    EXPECT_NE(reinterpret_cast<uint8_t*>(NULL), export_addr);

    // Edit the first byte of the function.
    uint8_t new_val = (*export_addr) + 1;
    SIZE_T bytes_written = 0;
    WriteProcessMemory(GetCurrentProcess(),
                       export_addr,
                       reinterpret_cast<void*>(&new_val),
                       1,
                       &bytes_written);
    EXPECT_EQ(1, bytes_written);
  }

  base::ScopedNativeLibrary mem_dll_handle_;
  base::MemoryMappedFile disk_dll_handle_;
  scoped_ptr<base::win::PEImageAsData> disk_peimage_ptr_;
  scoped_ptr<base::win::PEImage> mem_peimage_ptr_;
};

// Don't run these tests under AddressSanitizer as it patches the modules on
// startup, thus interferes with all these test expectations.
#if !defined(ADDRESS_SANITIZER)
TEST_F(SafeBrowsingModuleVerifierWinTest, VerifyModuleUnmodified) {
  std::set<std::string> modified_exports;
  int num_bytes = 0;
  // Call VerifyModule before the module has been loaded, should fail.
  ASSERT_EQ(MODULE_STATE_UNKNOWN,
            VerifyModule(kTestDllNames[0], &modified_exports, &num_bytes));
  ASSERT_EQ(0, modified_exports.size());

  // On loading, the module should be identical (up to relocations) in memory as
  // on disk.
  SetUpTestDllAndPEImages();
  EXPECT_EQ(MODULE_STATE_UNMODIFIED,
            VerifyModule(kTestDllNames[0], &modified_exports, &num_bytes));
  EXPECT_EQ(0, modified_exports.size());
}

TEST_F(SafeBrowsingModuleVerifierWinTest, VerifyModuleModified) {
  std::set<std::string> modified_exports;
  int num_bytes = 0;
  // Confirm the module is identical in memory as on disk before we begin.
  SetUpTestDllAndPEImages();
  ASSERT_EQ(MODULE_STATE_UNMODIFIED,
            VerifyModule(kTestDllNames[0], &modified_exports, &num_bytes));

  uint8_t* mem_code_addr = NULL;
  uint8_t* disk_code_addr = NULL;
  uint32_t code_size = 0;
  ASSERT_TRUE(GetCodeAddrsAndSize(*mem_peimage_ptr_,
                                  *disk_peimage_ptr_,
                                  &mem_code_addr,
                                  &disk_code_addr,
                                  &code_size));

  // Edit the first byte of the code section of the module (this may be before
  // the address of any export).
  uint8_t new_val = (*mem_code_addr) + 1;
  SIZE_T bytes_written = 0;
  WriteProcessMemory(GetCurrentProcess(),
                     mem_code_addr,
                     reinterpret_cast<void*>(&new_val),
                     1,
                     &bytes_written);
  EXPECT_EQ(1, bytes_written);

  // VerifyModule should detect the change.
  EXPECT_EQ(MODULE_STATE_MODIFIED,
            VerifyModule(kTestDllNames[0], &modified_exports, &num_bytes));
}

TEST_F(SafeBrowsingModuleVerifierWinTest, VerifyModuleExportModified) {
  std::set<std::string> modified_exports;
  int num_bytes = 0;
  // Confirm the module is identical in memory as on disk before we begin.
  SetUpTestDllAndPEImages();
  ASSERT_EQ(MODULE_STATE_UNMODIFIED,
            VerifyModule(kTestDllNames[0], &modified_exports, &num_bytes));
  modified_exports.clear();

  // Edit the exported function, VerifyModule should now return the function
  // name in modified_exports.
  EditExport();
  EXPECT_EQ(MODULE_STATE_MODIFIED,
            VerifyModule(kTestDllNames[0], &modified_exports, &num_bytes));
  ASSERT_EQ(1, modified_exports.size());
  EXPECT_EQ(0, std::string(kTestExportName).compare(*modified_exports.begin()));
}

TEST_F(SafeBrowsingModuleVerifierWinTest, NewVerifyModuleUnmodified) {
  ClientIncidentReport_EnvironmentData_Process_ModuleState state;
  // Call VerifyModule before the module has been loaded, should fail.
  VerificationResult result = NewVerifyModule(kTestDllNames[0], &state);

  ASSERT_EQ(MODULE_STATE_UNKNOWN, result.state);
  EXPECT_EQ(0, result.num_bytes_different);

  // On loading, the module should be identical (up to relocations) in memory as
  // on disk.
  SetUpTestDllAndPEImages();
  result = NewVerifyModule(kTestDllNames[0], &state);
  EXPECT_EQ(MODULE_STATE_UNMODIFIED, result.state);
  EXPECT_EQ(0, state.modification_size());
  EXPECT_EQ(0, result.num_bytes_different);
}

TEST_F(SafeBrowsingModuleVerifierWinTest, NewVerifyModuleModified) {
  ClientIncidentReport_EnvironmentData_Process_ModuleState state;

  SetUpTestDllAndPEImages();
  VerificationResult result = NewVerifyModule(kTestDllNames[0], &state);
  ASSERT_EQ(MODULE_STATE_UNMODIFIED, result.state);
  EXPECT_EQ(0, result.num_bytes_different);

  uint8_t* mem_code_addr = NULL;
  uint8_t* disk_code_addr = NULL;
  uint32_t code_size = 0;
  ASSERT_TRUE(GetCodeAddrsAndSize(*mem_peimage_ptr_,
                                  *disk_peimage_ptr_,
                                  &mem_code_addr,
                                  &disk_code_addr,
                                  &code_size));

  uint8_t new_first_byte = (*mem_code_addr) + 1;
  SIZE_T bytes_written = 0;
  WriteProcessMemory(GetCurrentProcess(),
                     mem_code_addr,
                     reinterpret_cast<void*>(&new_first_byte),
                     1,
                     &bytes_written);
  EXPECT_EQ(1, bytes_written);

  size_t modification_offset = code_size - 1;
  uint8_t* last_byte_addr = mem_code_addr + modification_offset;
  uint8_t new_last_byte = (*last_byte_addr) + 1;
  bytes_written = 0;
  WriteProcessMemory(GetCurrentProcess(),
                     last_byte_addr,
                     reinterpret_cast<void*>(&new_last_byte),
                     1,
                     &bytes_written);
  EXPECT_EQ(1, bytes_written);
  result = NewVerifyModule(kTestDllNames[0], &state);
  EXPECT_EQ(MODULE_STATE_MODIFIED, result.state);

  EXPECT_EQ(2, result.num_bytes_different);

  EXPECT_EQ(2, state.modification_size());
  size_t expected_file_offset =
      disk_code_addr - reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module());
  EXPECT_EQ(expected_file_offset, state.modification(0).file_offset());
  EXPECT_EQ(1, state.modification(0).byte_count());
  EXPECT_EQ(new_first_byte, (uint8_t)state.modification(0).modified_bytes()[0]);

  expected_file_offset = (disk_code_addr + modification_offset) -
      reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module());
  EXPECT_EQ(expected_file_offset, state.modification(1).file_offset());
  EXPECT_EQ(1, state.modification(1).byte_count());
  EXPECT_EQ(new_last_byte, (uint8_t)state.modification(1).modified_bytes()[0]);
}

TEST_F(SafeBrowsingModuleVerifierWinTest, NewVerifyModuleLongModification) {
  ClientIncidentReport_EnvironmentData_Process_ModuleState state;

  SetUpTestDllAndPEImages();
  VerificationResult result = NewVerifyModule(kTestDllNames[0], &state);
  ASSERT_EQ(MODULE_STATE_UNMODIFIED, result.state);

  EXPECT_EQ(0, result.num_bytes_different);

  uint8_t* mem_code_addr = NULL;
  uint8_t* disk_code_addr = NULL;
  uint32_t code_size = 0;
  ASSERT_TRUE(GetCodeAddrsAndSize(*mem_peimage_ptr_,
                                  *disk_peimage_ptr_,
                                  &mem_code_addr,
                                  &disk_code_addr,
                                  &code_size));

  const size_t modification_size = 256;
  // Write the modification at the end so it's not overlapping relocations
  const size_t modification_offset = code_size - modification_size;
  uint8_t modification[modification_size] { 0 };
  for (size_t i = 0; i < modification_size; ++i) {
    modification[i] = disk_code_addr[modification_offset + i] + 1;
  }

  SIZE_T bytes_written = 0;
  uint8_t* write_addr = mem_code_addr + modification_offset;
  WriteProcessMemory(GetCurrentProcess(),
                     write_addr,
                     reinterpret_cast<void*>(&modification),
                     modification_size,
                     &bytes_written);
  EXPECT_EQ(modification_size, bytes_written);

  result = NewVerifyModule(kTestDllNames[0], &state);
  EXPECT_EQ(MODULE_STATE_MODIFIED, result.state);

  EXPECT_EQ(modification_size, result.num_bytes_different);

  EXPECT_EQ(1, state.modification_size());
  EXPECT_EQ(modification_size, state.modification(0).byte_count());

  size_t expected_file_offset = disk_code_addr + modification_offset -
      reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module());
  EXPECT_EQ(expected_file_offset, state.modification(0).file_offset());

  for (size_t i = 0; i < modification_size; ++i) {
    EXPECT_EQ((uint8_t)state.modification(0).modified_bytes()[i],
              (uint8_t)(disk_code_addr[modification_offset + i] + 1));
  }
}

TEST_F(SafeBrowsingModuleVerifierWinTest, NewVerifyModuleRelocOverlap) {
  ClientIncidentReport_EnvironmentData_Process_ModuleState state;

  SetUpTestDllAndPEImages();
  VerificationResult result = NewVerifyModule(kTestDllNames[0], &state);
  ASSERT_EQ(MODULE_STATE_UNMODIFIED, result.state);

  EXPECT_EQ(0, result.num_bytes_different);

  uint8_t* mem_code_addr = NULL;
  uint8_t* disk_code_addr = NULL;
  uint32_t code_size = 0;
  ASSERT_TRUE(GetCodeAddrsAndSize(*mem_peimage_ptr_,
                                  *disk_peimage_ptr_,
                                  &mem_code_addr,
                                  &disk_code_addr,
                                  &code_size));

  const size_t modification_size = 256;
  // Write the modification at a point where there are relocations
  const size_t modification_offset = 0xF50;
  uint8_t modification[modification_size] { 0 };
  for (size_t i = 0; i < modification_size; ++i) {
    modification[i] = disk_code_addr[modification_offset + i] + 1;
  }

  SIZE_T bytes_written = 0;
  uint8_t* write_addr = mem_code_addr + modification_offset;
  WriteProcessMemory(GetCurrentProcess(),
                     write_addr,
                     reinterpret_cast<void*>(&modification),
                     modification_size,
                     &bytes_written);
  EXPECT_EQ(modification_size, bytes_written);

  result = NewVerifyModule(kTestDllNames[0], &state);
  EXPECT_EQ(MODULE_STATE_MODIFIED, result.state);

  EXPECT_EQ(modification_size, result.num_bytes_different);

  // There should be more than one modification because there were relocations
  // in the modified range.
  EXPECT_EQ((size_t)1, state.modification_size());

  EXPECT_EQ(modification_size, state.modification(0).byte_count());
}

#endif  // ADDRESS_SANITIZER

}  // namespace safe_browsing
