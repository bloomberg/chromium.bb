// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/module_integrity_verifier_win.h"

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/sparse_histogram.h"
#include "base/scoped_native_library.h"
#include "base/win/pe_image.h"
#include "build/build_config.h"

namespace safe_browsing {

struct ModuleVerificationState {
  explicit ModuleVerificationState(HMODULE hModule);
  ~ModuleVerificationState();

  base::win::PEImageAsData disk_peimage;

  // The module's preferred base address minus the base address it actually
  // loaded at.
  intptr_t image_base_delta;

  // The location of the disk_peimage module's code section minus that of the
  // mem_peimage module's code section.
  intptr_t code_section_delta;

  // The bytes corrected by relocs.
  base::hash_set<uintptr_t> reloc_addr;

  // Set true if the relocation table contains a reloc of type that we don't
  // currently handle.
  bool unknown_reloc_type;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModuleVerificationState);
};

ModuleVerificationState::ModuleVerificationState(HMODULE hModule)
    : disk_peimage(hModule),
      image_base_delta(0),
      code_section_delta(0),
      reloc_addr(),
      unknown_reloc_type(false) {
}

ModuleVerificationState::~ModuleVerificationState() {
}

namespace {

struct Export {
  Export(void* addr, const std::string& name);
  ~Export();

  bool operator<(const Export& other) const;

  void* addr;
  std::string name;
};

Export::Export(void* addr, const std::string& name) : addr(addr), name(name) {
}

Export::~Export() {
}

bool Export::operator<(const Export& other) const {
  return addr < other.addr;
}

bool ByteAccountedForByReloc(uint8_t* byte_addr,
                             const ModuleVerificationState& state) {
  return ((state.reloc_addr.count(reinterpret_cast<uintptr_t>(byte_addr))) > 0);
}

// Checks each byte in the module's code section again the corresponding byte on
// disk, returning the number of bytes differing between the two.  Also adds the
// names of any modfied functions exported by name to |modified_exports|.
// |exports| must be sorted.
int ExamineBytesDiffInMemory(uint8_t* disk_code_start,
                             uint8_t* mem_code_start,
                             uint32_t code_size,
                             const std::vector<Export>& exports,
                             const ModuleVerificationState& state,
                             std::set<std::string>* modified_exports) {
  int bytes_different = 0;
  std::vector<Export>::const_iterator export_it = exports.begin();

  for (uint8_t* end = mem_code_start + code_size; mem_code_start != end;
       ++mem_code_start) {
    if ((*disk_code_start++ != *mem_code_start) &&
        !ByteAccountedForByReloc(mem_code_start, state)) {
      // We get the largest export address still smaller than |addr|.  It is
      // possible that |addr| belongs to some nonexported function located
      // between this export and the following one.
      Export addr(reinterpret_cast<void*>(mem_code_start), std::string());
      std::vector<Export>::const_iterator modified_export_it =
          std::upper_bound(export_it, exports.end(), addr);

      if (modified_export_it != exports.begin())
        modified_exports->insert((modified_export_it - 1)->name);
      ++bytes_different;

      // No later byte can belong to an earlier export.
      export_it = modified_export_it;
    }
  }
  return bytes_different;
}

// Adds to |state->reloc_addr| the bytes of the pointer at |address| that are
// corrected by adding |image_base_delta|.
void AddBytesCorrectedByReloc(uintptr_t address,
                              ModuleVerificationState* state) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
#  define OFFSET(i) i
#else
#  define OFFSET(i) (sizeof(uintptr_t) - i)
#endif

  uintptr_t orig_mem_value = *reinterpret_cast<uintptr_t*>(address);
  uintptr_t fixed_mem_value = orig_mem_value + state->image_base_delta;
  uintptr_t disk_value =
      *reinterpret_cast<uintptr_t*>(address + state->code_section_delta);

  uintptr_t diff_before = orig_mem_value ^ disk_value;
  uintptr_t shared_after = ~(fixed_mem_value ^ disk_value);
  int i = 0;
  for (uintptr_t fixed = diff_before & shared_after; fixed; fixed >>= 8, ++i) {
    if (fixed & 0xFF)
      state->reloc_addr.insert(address + OFFSET(i));
  }
#undef OFFSET
}

bool AddrIsInCodeSection(void* address,
                         uint8_t* code_addr,
                         uint32_t code_size) {
  return (code_addr <= address && address < code_addr + code_size);
}

bool EnumRelocsCallback(const base::win::PEImage& mem_peimage,
                        WORD type,
                        void* address,
                        void* cookie) {
  ModuleVerificationState* state =
      reinterpret_cast<ModuleVerificationState*>(cookie);

  uint8_t* mem_code_addr = NULL;
  uint8_t* disk_code_addr = NULL;
  uint32_t code_size = 0;
  if (!GetCodeAddrsAndSize(mem_peimage,
                           state->disk_peimage,
                           &mem_code_addr,
                           &disk_code_addr,
                           &code_size))
    return false;

  // If not in the code section return true to continue to the next reloc.
  if (!AddrIsInCodeSection(address, mem_code_addr, code_size))
    return true;

  UMA_HISTOGRAM_SPARSE_SLOWLY("SafeBrowsing.ModuleBaseRelocation", type);

  switch (type) {
    case IMAGE_REL_BASED_HIGHLOW: {
      AddBytesCorrectedByReloc(reinterpret_cast<uintptr_t>(address), state);
      break;
    }
    case IMAGE_REL_BASED_ABSOLUTE:
      // Absolute type relocations are a noop, sometimes used to pad a section
      // of relocations.
      break;
    default: {
      // TODO(krstnmnlsn): Find a reliable description of the behaviour of the
      // remaining types of relocation and handle them.
      state->unknown_reloc_type = true;
      break;
    }
  }
  return true;
}

bool EnumExportsCallback(const base::win::PEImage& mem_peimage,
                         DWORD ordinal,
                         DWORD hint,
                         LPCSTR name,
                         PVOID function_addr,
                         LPCSTR forward,
                         PVOID cookie) {
  std::vector<Export>* exports = reinterpret_cast<std::vector<Export>*>(cookie);
  if (name)
    exports->push_back(Export(function_addr, std::string(name)));
  return true;
}

}  // namespace

bool GetCodeAddrsAndSize(const base::win::PEImage& mem_peimage,
                         const base::win::PEImageAsData& disk_peimage,
                         uint8_t** mem_code_addr,
                         uint8_t** disk_code_addr,
                         uint32_t* code_size) {
  DWORD base_of_code = mem_peimage.GetNTHeaders()->OptionalHeader.BaseOfCode;

  // Get the address and size of the code section in the loaded module image.
  PIMAGE_SECTION_HEADER mem_code_header =
      mem_peimage.GetImageSectionFromAddr(mem_peimage.RVAToAddr(base_of_code));
  if (mem_code_header == NULL)
    return false;
  *mem_code_addr = reinterpret_cast<uint8_t*>(
      mem_peimage.RVAToAddr(mem_code_header->VirtualAddress));
  // If the section is padded with zeros when mapped then |VirtualSize| can be
  // larger.  Alternatively, |SizeOfRawData| can be rounded up to align
  // according to OptionalHeader.FileAlignment.
  *code_size = std::min(mem_code_header->Misc.VirtualSize,
                        mem_code_header->SizeOfRawData);

  // Get the address of the code section in the module mapped as data from disk.
  DWORD disk_code_offset = 0;
  if (!mem_peimage.ImageAddrToOnDiskOffset(
          reinterpret_cast<void*>(*mem_code_addr), &disk_code_offset))
    return false;
  *disk_code_addr =
      reinterpret_cast<uint8_t*>(disk_peimage.module()) + disk_code_offset;
  return true;
}

ModuleState VerifyModule(const wchar_t* module_name,
                         std::set<std::string>* modified_exports) {
  // Get module handle, load a copy from disk as data and create PEImages.
  HMODULE module_handle = NULL;
  if (!GetModuleHandleEx(0, module_name, &module_handle))
    return MODULE_STATE_UNKNOWN;
  base::ScopedNativeLibrary native_library(module_handle);

  WCHAR module_path[MAX_PATH] = {};
  DWORD length =
      GetModuleFileName(module_handle, module_path, arraysize(module_path));
  if (!length || length == arraysize(module_path))
    return MODULE_STATE_UNKNOWN;

  base::MemoryMappedFile mapped_module;
  if (!mapped_module.Initialize(base::FilePath(module_path)))
    return MODULE_STATE_UNKNOWN;
  ModuleVerificationState state(
      reinterpret_cast<HMODULE>(const_cast<uint8*>(mapped_module.data())));

  base::win::PEImage mem_peimage(module_handle);
  if (!mem_peimage.VerifyMagic() || !state.disk_peimage.VerifyMagic())
    return MODULE_STATE_UNKNOWN;

  // Get the list of exports.
  std::vector<Export> exports;
  mem_peimage.EnumExports(EnumExportsCallback, &exports);
  std::sort(exports.begin(), exports.end());

  // Get the addresses of the code sections then calculate |code_section_delta|
  // and |image_base_delta|.
  uint8_t* mem_code_addr = NULL;
  uint8_t* disk_code_addr = NULL;
  uint32_t code_size = 0;
  if (!GetCodeAddrsAndSize(mem_peimage,
                           state.disk_peimage,
                           &mem_code_addr,
                           &disk_code_addr,
                           &code_size))
    return MODULE_STATE_UNKNOWN;

  state.code_section_delta = disk_code_addr - mem_code_addr;

  uint8_t* preferred_image_base = reinterpret_cast<uint8_t*>(
      state.disk_peimage.GetNTHeaders()->OptionalHeader.ImageBase);
  state.image_base_delta =
      preferred_image_base - reinterpret_cast<uint8_t*>(mem_peimage.module());

  // Get the relocations.
  mem_peimage.EnumRelocs(EnumRelocsCallback, &state);
  if (state.unknown_reloc_type)
    return MODULE_STATE_UNKNOWN;

  // Count the modified bytes (after accounting for relocs) and get the set of
  // modified functions.
  int num_bytes_different = ExamineBytesDiffInMemory(disk_code_addr,
                                                     mem_code_addr,
                                                     code_size,
                                                     exports,
                                                     state,
                                                     modified_exports);

  return num_bytes_different ? MODULE_STATE_MODIFIED : MODULE_STATE_UNMODIFIED;
}

}  // namespace safe_browsing
