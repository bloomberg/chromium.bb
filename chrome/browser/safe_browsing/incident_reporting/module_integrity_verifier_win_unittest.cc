// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_verifier_win.h"

#include <algorithm>
#include <functional>
#include <map>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/native_library.h"
#include "base/scoped_native_library.h"
#include "base/win/pe_image.h"
#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_unittest_util_win.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

// A scoper that makes a modification at a given address when constructed, and
// reverts it upon destruction.
template <size_t ModificationLength>
class ScopedModuleModifier {
 public:
  explicit ScopedModuleModifier(uint8_t* address) : address_(address) {
    uint8_t modification[ModificationLength];
    std::transform(address, address + ModificationLength, &modification[0],
                   std::bind2nd(std::plus<uint8_t>(), 1U));
    SIZE_T bytes_written = 0;
    EXPECT_NE(0, WriteProcessMemory(GetCurrentProcess(),
                                    address,
                                    &modification[0],
                                    ModificationLength,
                                    &bytes_written));
    EXPECT_EQ(ModificationLength, bytes_written);
  }

  ~ScopedModuleModifier() {
    uint8_t modification[ModificationLength];
    std::transform(address_, address_ + ModificationLength, &modification[0],
                   std::bind2nd(std::minus<uint8_t>(), 1U));
    SIZE_T bytes_written = 0;
    EXPECT_NE(0, WriteProcessMemory(GetCurrentProcess(),
                                    address_,
                                    &modification[0],
                                    ModificationLength,
                                    &bytes_written));
    EXPECT_EQ(ModificationLength, bytes_written);
  }

 private:
  uint8_t* address_;

  DISALLOW_COPY_AND_ASSIGN(ScopedModuleModifier);
};

class SafeBrowsingModuleVerifierWinTest : public testing::Test {
 protected:
  using ModuleState = ClientIncidentReport_EnvironmentData_Process_ModuleState;

  // A mapping of an export name to the sequence of modifications for it.
  using ExportNameToModifications =
      std::map<std::string, std::vector<const ModuleState::Modification*>>;

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

  // Returns the address of the named function exported by the test dll.
  uint8_t* GetAddressOfExport(const char* export_name) {
    HMODULE mem_handle;
    GetMemModuleHandle(&mem_handle);
    uint8_t* export_addr =
        reinterpret_cast<uint8_t*>(GetProcAddress(mem_handle, export_name));
    EXPECT_NE(nullptr, export_addr);
    return export_addr;
  }

  // Replaces the contents of |modification_map| with pointers to those in
  // |state|. |state| must outlive |modification_map|.
  static void BuildModificationMap(
      const ModuleState& state,
      ExportNameToModifications* modification_map) {
    modification_map->clear();
    std::string export_name;
    for (auto& modification : state.modification()) {
      if (!modification.has_export_name())
        export_name.clear();
      else
        export_name = modification.export_name();
      (*modification_map)[export_name].push_back(&modification);
    }
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
  ScopedModuleModifier<1> mod(mem_code_addr);

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
  ScopedModuleModifier<1> mod(GetAddressOfExport(kTestExportName));
  EXPECT_EQ(MODULE_STATE_MODIFIED,
            VerifyModule(kTestDllNames[0], &modified_exports, &num_bytes));
  ASSERT_EQ(1, modified_exports.size());
  EXPECT_EQ(0, std::string(kTestExportName).compare(*modified_exports.begin()));
}

TEST_F(SafeBrowsingModuleVerifierWinTest, NewVerifyModuleUnmodified) {
  ModuleState state;
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
  ModuleState state;

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

  ScopedModuleModifier<1> mod(mem_code_addr);

  size_t modification_offset = code_size - 1;
  ScopedModuleModifier<1> mod2(mem_code_addr + modification_offset);

  result = NewVerifyModule(kTestDllNames[0], &state);
  EXPECT_EQ(MODULE_STATE_MODIFIED, result.state);

  EXPECT_EQ(2, result.num_bytes_different);

  EXPECT_EQ(2, state.modification_size());
  size_t expected_file_offset =
      disk_code_addr - reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module());
  EXPECT_EQ(expected_file_offset, state.modification(0).file_offset());
  EXPECT_EQ(1, state.modification(0).byte_count());
  EXPECT_EQ(mem_code_addr[0],
            (uint8_t)state.modification(0).modified_bytes()[0]);

  expected_file_offset = (disk_code_addr + modification_offset) -
      reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module());
  EXPECT_EQ(expected_file_offset, state.modification(1).file_offset());
  EXPECT_EQ(1, state.modification(1).byte_count());
  EXPECT_EQ(mem_code_addr[modification_offset],
            (uint8_t)state.modification(1).modified_bytes()[0]);
}

TEST_F(SafeBrowsingModuleVerifierWinTest, NewVerifyModuleLongModification) {
  ModuleState state;

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

  const size_t kModificationSize = 256;
  // Write the modification at the end so it's not overlapping relocations
  const size_t modification_offset = code_size - kModificationSize;
  ScopedModuleModifier<kModificationSize> mod(
      mem_code_addr + modification_offset);

  result = NewVerifyModule(kTestDllNames[0], &state);
  EXPECT_EQ(MODULE_STATE_MODIFIED, result.state);

  EXPECT_EQ(kModificationSize, result.num_bytes_different);

  EXPECT_EQ(1, state.modification_size());
  EXPECT_EQ(kModificationSize, state.modification(0).byte_count());

  size_t expected_file_offset = disk_code_addr + modification_offset -
      reinterpret_cast<uint8_t*>(disk_peimage_ptr_->module());
  EXPECT_EQ(expected_file_offset, state.modification(0).file_offset());

  EXPECT_EQ(
      std::string(mem_code_addr + modification_offset,
                  mem_code_addr + modification_offset + kModificationSize),
      state.modification(0).modified_bytes());
}

TEST_F(SafeBrowsingModuleVerifierWinTest, NewVerifyModuleRelocOverlap) {
  ModuleState state;

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

  // Modify the first hunk of the code, which contains many relocs.
  const size_t kModificationSize = 256;
  ScopedModuleModifier<kModificationSize> mod(mem_code_addr);

  result = NewVerifyModule(kTestDllNames[0], &state);
  EXPECT_EQ(MODULE_STATE_MODIFIED, result.state);

  EXPECT_EQ(kModificationSize, result.num_bytes_different);

  // Modifications across the relocs should have been coalesced into one.
  ASSERT_EQ(1U, state.modification_size());
  ASSERT_EQ(kModificationSize, state.modification(0).byte_count());
  ASSERT_EQ(kModificationSize, state.modification(0).modified_bytes().size());
  EXPECT_EQ(std::string(mem_code_addr, mem_code_addr + kModificationSize),
            state.modification(0).modified_bytes());
}

TEST_F(SafeBrowsingModuleVerifierWinTest, NewVerifyModuleExportModified) {
  ModuleState state;

  // Confirm the module is identical in memory as on disk before we begin.
  SetUpTestDllAndPEImages();
  VerificationResult result = NewVerifyModule(kTestDllNames[0], &state);
  ASSERT_EQ(MODULE_STATE_UNMODIFIED, result.state);
  state.Clear();

  // Edit one exported function. VerifyModule should now return the function
  // name in the modification.
  ScopedModuleModifier<1> mod(GetAddressOfExport(kTestExportName));
  result = NewVerifyModule(kTestDllNames[0], &state);
  EXPECT_EQ(MODULE_STATE_MODIFIED, result.state);
  ASSERT_EQ(1, state.modification_size());

  // Extract the offset of this modification.
  ExportNameToModifications modification_map;
  BuildModificationMap(state, &modification_map);
  ASSERT_EQ(1U, modification_map[kTestExportName].size());
  uint32_t export_offset = modification_map[kTestExportName][0]->file_offset();

  // Edit another exported function. VerifyModule should now report both.
  state.Clear();
  ScopedModuleModifier<1> mod2(GetAddressOfExport(kTestDllMainExportName));
  result = NewVerifyModule(kTestDllNames[0], &state);
  EXPECT_EQ(MODULE_STATE_MODIFIED, result.state);
  ASSERT_EQ(2, state.modification_size());

  // The first modification should be present and unmodified.
  BuildModificationMap(state, &modification_map);
  ASSERT_EQ(1U, modification_map[kTestExportName].size());
  ASSERT_EQ(export_offset, modification_map[kTestExportName][0]->file_offset());

  // The second modification should be present and different than the first.
  ASSERT_EQ(1U, modification_map[kTestDllMainExportName].size());
  ASSERT_NE(export_offset,
            modification_map[kTestDllMainExportName][0]->file_offset());

  // Now make another edit at the very end of the code section. This should be
  // attributed to the last export.
  uint8_t* mem_code_addr = nullptr;
  uint8_t* disk_code_addr = nullptr;
  uint32_t code_size = 0;
  ASSERT_TRUE(GetCodeAddrsAndSize(*mem_peimage_ptr_,
                                  *disk_peimage_ptr_,
                                  &mem_code_addr,
                                  &disk_code_addr,
                                  &code_size));
  ScopedModuleModifier<1> mod3(mem_code_addr + code_size - 1);

  state.Clear();
  result = NewVerifyModule(kTestDllNames[0], &state);
  EXPECT_EQ(MODULE_STATE_MODIFIED, result.state);
  ASSERT_EQ(3, state.modification_size());

  // One of the two exports now has two modifications.
  BuildModificationMap(state, &modification_map);
  ASSERT_EQ(2U, modification_map.size());
  ASSERT_EQ(3U, (modification_map.begin()->second.size() +
                 (++modification_map.begin())->second.size()));
}
#endif  // ADDRESS_SANITIZER

}  // namespace safe_browsing
