// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/elf_loader.h"

#include <elf.h>
#include <link.h>

#include <cstring>
#include <string>
#include <sys/mman.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector_trusted_mem.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"

// Extracted from native_client/src/trusted/service_runtime/nacl_config.h.
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
# if NACL_BUILD_SUBARCH == 64
#  define NACL_ELF_E_MACHINE EM_X86_64
# elif NACL_BUILD_SUBARCH == 32
#  define NACL_ELF_E_MACHINE EM_386
# else
#  error Unknown platform.
# endif
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
# define NACL_ELF_E_MACHINE EM_ARM
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_mips
# define NACL_ELF_E_MACHINE EM_MIPS
#else
# error Unknown platform.
#endif

namespace nacl {
namespace nonsfi {
namespace {

// Page size for non-SFI Mode.
const ElfW(Addr) kNonSfiPageSize = 4096;
const ElfW(Addr) kNonSfiPageMask = kNonSfiPageSize - 1;

NaClErrorCode ValidateElfHeader(const ElfW(Ehdr)& ehdr) {
  if (std::memcmp(ehdr.e_ident, ELFMAG, SELFMAG)) {
    LOG(ERROR) << "Bad elf magic";
    return LOAD_BAD_ELF_MAGIC;
  }

#if NACL_BUILD_SUBARCH == 32
  if (ehdr.e_ident[EI_CLASS] != ELFCLASS32) {
    LOG(ERROR) << "Bad elf class";
    return LOAD_NOT_32_BIT;
  }
#elif NACL_BUILD_SUBARCH == 64
  if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
    LOG(ERROR) << "Bad elf class";
    return LOAD_NOT_64_BIT;
  }
#else
# error Unknown platform.
#endif

  if (ehdr.e_type != ET_DYN) {
    LOG(ERROR) << "Not a relocatable ELF object (not ET_DYN)";
    return LOAD_NOT_EXEC;
  }

  if (ehdr.e_machine != NACL_ELF_E_MACHINE) {
    LOG(ERROR) << "Bad machine: "
               << base::HexEncode(&ehdr.e_machine, sizeof(ehdr.e_machine));
    return LOAD_BAD_MACHINE;
  }

  if (ehdr.e_version != EV_CURRENT) {
    LOG(ERROR) << "Bad elf version: "
               << base::HexEncode(&ehdr.e_version, sizeof(ehdr.e_version));
  }

  return LOAD_OK;
}

// Returns the address of the page starting at address 'addr' for non-SFI mode.
ElfW(Addr) GetPageStart(ElfW(Addr) addr) {
  return addr & ~kNonSfiPageMask;
}

// Returns the offset of address 'addr' in its memory page. In other words,
// this equals to 'addr' - GetPageStart(addr).
ElfW(Addr) GetPageOffset(ElfW(Addr) addr) {
  return addr & kNonSfiPageMask;
}

// Returns the address of the next page after address 'addr', unless 'addr' is
// at the start of a page. This equals to:
//   addr == GetPageStart(addr) ? addr : GetPageStart(addr) + kNonSfiPageSize
ElfW(Addr) GetPageEnd(ElfW(Addr) addr) {
  return GetPageStart(addr + kNonSfiPageSize - 1);
}

// Converts the pflags (in phdr) to mmap's prot flags.
int PFlagsToProt(int pflags) {
  return ((pflags & PF_X) ? PROT_EXEC : 0) |
         ((pflags & PF_R) ? PROT_READ : 0) |
         ((pflags & PF_W) ? PROT_WRITE : 0);
}

// Converts the pflags (in phdr) to NaCl ABI's prot flags.
int PFlagsToNaClProt(int pflags) {
  return ((pflags & PF_X) ? NACL_ABI_PROT_EXEC : 0) |
         ((pflags & PF_R) ? NACL_ABI_PROT_READ : 0) |
         ((pflags & PF_W) ? NACL_ABI_PROT_WRITE : 0);
}

// Returns the load size for the given phdrs, or 0 on error.
ElfW(Addr) GetLoadSize(const ElfW(Phdr)* phdrs, int phnum) {
  ElfW(Addr) begin = ~static_cast<ElfW(Addr)>(0);
  ElfW(Addr) end = 0;

  for (int i = 0; i < phnum; ++i) {
    const ElfW(Phdr)& phdr = phdrs[i];
    if (phdr.p_type != PT_LOAD) {
      // Do nothing for non PT_LOAD header.
      continue;
    }

    begin = std::min(begin, phdr.p_vaddr);
    end = std::max(end, phdr.p_vaddr + phdr.p_memsz);
  }

  if (begin > end) {
    // The end address looks overflowing, or PT_LOAD is not found.
    return 0;
  }

  return GetPageEnd(end) - GetPageStart(begin);
}

// Reserves the memory for the given phdrs, and stores the memory bias to the
// load_bias.
NaClErrorCode ReserveMemory(const ElfW(Phdr)* phdrs,
                            int phnum,
                            ElfW(Addr)* load_bias) {
  ElfW(Addr) size = GetLoadSize(phdrs, phnum);
  if (size == 0) {
    LOG(ERROR) << "ReserveMemory failed to calculate size";
    return LOAD_UNLOADABLE;
  }

  // Make sure that the given program headers represents PIE binary.
  for (int i = 0; i < phnum; ++i) {
    if (phdrs[i].p_type == PT_LOAD) {
      // Here, phdrs[i] is the first loadable segment.
      if (phdrs[i].p_vaddr != 0) {
        // The binary is not PIE (i.e. needs to be loaded onto fixed addressed
        // memory. We don't support such a case.
        LOG(ERROR)
            << "ReserveMemory: Non-PIE binary loading is not supported.";
        return LOAD_UNLOADABLE;
      }
      break;
    }
  }

  void* start = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (start == MAP_FAILED) {
    LOG(ERROR) << "ReserveMemory: failed to mmap.";
    return LOAD_NO_MEMORY;
  }

  *load_bias = reinterpret_cast<ElfW(Addr)>(start);
  return LOAD_OK;
}

NaClErrorCode LoadSegments(
    const ElfW(Phdr)* phdrs, int phnum, ElfW(Addr) load_bias,
    struct NaClDesc* descriptor) {
  for (int i = 0; i < phnum; ++i) {
    const ElfW(Phdr)& phdr = phdrs[i];
    if (phdr.p_type != PT_LOAD) {
      // Not a load target.
      continue;
    }

    // Addresses on the memory.
    ElfW(Addr) seg_start = phdr.p_vaddr + load_bias;
    ElfW(Addr) seg_end = seg_start + phdr.p_memsz;
    ElfW(Addr) seg_page_start = GetPageStart(seg_start);
    ElfW(Addr) seg_page_end = GetPageEnd(seg_end);
    ElfW(Addr) seg_file_end = seg_start + phdr.p_filesz;

    // Addresses on the file content.
    ElfW(Addr) file_start = phdr.p_offset;
    ElfW(Addr) file_end = file_start + phdr.p_filesz;
    ElfW(Addr) file_page_start = GetPageStart(file_start);

    uintptr_t seg_addr = (*NACL_VTBL(NaClDesc, descriptor)->Map)(
        descriptor,
        NaClDescEffectorTrustedMem(),
        reinterpret_cast<void *>(seg_page_start),
        file_end - file_page_start,
        PFlagsToNaClProt(phdr.p_flags),
        NACL_ABI_MAP_PRIVATE | NACL_ABI_MAP_FIXED,
        file_page_start);
    if (NaClPtrIsNegErrno(&seg_addr)) {
      LOG(ERROR) << "LoadSegments: [" << i << "] mmap failed, " << seg_addr;
      return LOAD_NO_MEMORY;
    }

    // Handle the BSS: fill Zero between the segment end and the page boundary
    // if necessary (i.e. if the segment doesn't end on a page boundary).
    ElfW(Addr) seg_file_end_offset = GetPageOffset(seg_file_end);
    if ((phdr.p_flags & PF_W) && seg_file_end_offset > 0) {
      memset(reinterpret_cast<void *>(seg_file_end), 0,
             kNonSfiPageSize - seg_file_end_offset);
    }

    // Hereafter, seg_file_end is now the first page address after the file
    // content. If seg_end is larger, we need to zero anything between them.
    // This is done by using a private anonymous mmap for all extra pages.
    seg_file_end = GetPageEnd(seg_file_end);
    if (seg_page_end > seg_file_end) {
      void* zeromap = mmap(reinterpret_cast<void *>(seg_file_end),
                           seg_page_end - seg_file_end,
                           PFlagsToProt(phdr.p_flags),
                           MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE,
                           -1, 0);
      if (zeromap == MAP_FAILED) {
        LOG(ERROR) << "LoadSegments: [" << i << "] Failed to zeromap.";
        return LOAD_NO_MEMORY;
      }
    }
  }
  return LOAD_OK;
}

}  // namespace

struct ElfImage::Data {
  // Limit of elf program headers allowed.
  enum {
    MAX_PROGRAM_HEADERS = 128
  };

  ElfW(Ehdr) ehdr;
  ElfW(Phdr) phdrs[MAX_PROGRAM_HEADERS];
  ElfW(Addr) load_bias;
};

ElfImage::ElfImage() {
}

ElfImage::~ElfImage() {
}

uintptr_t ElfImage::entry_point() const {
  if (!data_) {
    LOG(DFATAL) << "entry_point must be called after Read().";
    return 0;
  }
  return data_->ehdr.e_entry + data_->load_bias;
}

NaClErrorCode ElfImage::Read(struct NaClDesc* descriptor) {
  DCHECK(!data_);

  ::scoped_ptr<Data> data(new Data);

  // Read elf header.
  ssize_t read_ret = (*NACL_VTBL(NaClDesc, descriptor)->PRead)(
      descriptor, &data->ehdr, sizeof(data->ehdr), 0);
  if (NaClSSizeIsNegErrno(&read_ret) ||
      static_cast<size_t>(read_ret) != sizeof(data->ehdr)) {
    LOG(ERROR) << "Could not load elf headers.";
    return LOAD_READ_ERROR;
  }

  NaClErrorCode error_code = ValidateElfHeader(data->ehdr);
  if (error_code != LOAD_OK)
    return error_code;

  // Read program headers.
  if (data->ehdr.e_phnum > Data::MAX_PROGRAM_HEADERS) {
    LOG(ERROR) << "Too many program headers";
    return LOAD_TOO_MANY_PROG_HDRS;
  }

  if (data->ehdr.e_phentsize != sizeof(data->phdrs[0])) {
    LOG(ERROR) << "Bad program headers size\n"
               << "  ehdr_.e_phentsize = " << data->ehdr.e_phentsize << "\n"
               << "  sizeof phdrs[0] = " << sizeof(data->phdrs[0]);
    return LOAD_BAD_PHENTSIZE;
  }

  size_t read_size = data->ehdr.e_phnum * data->ehdr.e_phentsize;
  read_ret = (*NACL_VTBL(NaClDesc, descriptor)->PRead)(
      descriptor, data->phdrs, read_size, data->ehdr.e_phoff);

  if (NaClSSizeIsNegErrno(&read_ret) ||
      static_cast<size_t>(read_ret) != read_size) {
    LOG(ERROR) << "Cannot load prog headers";
    return LOAD_READ_ERROR;
  }

  data_.swap(data);
  return LOAD_OK;
}

NaClErrorCode ElfImage::Load(struct NaClDesc* descriptor) {
  if (!data_) {
    LOG(DFATAL) << "ElfImage::Load() must be called after Read()";
    return LOAD_INTERNAL;
  }

  NaClErrorCode error =
      ReserveMemory(data_->phdrs, data_->ehdr.e_phnum, &data_->load_bias);
  if (error != LOAD_OK) {
    LOG(ERROR) << "ElfImage::Load: Failed to allocate memory";
    return error;
  }

  error = LoadSegments(
      data_->phdrs, data_->ehdr.e_phnum, data_->load_bias, descriptor);
  if (error != LOAD_OK) {
    LOG(ERROR) << "ElfImage::Load: Failed to load segments";
    return error;
  }

  return LOAD_OK;
}

}  // namespace nonsfi
}  // namespace nacl
