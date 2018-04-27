// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/elf_reader_linux.h"

#include <arpa/inet.h>
#include <elf.h>

#include <vector>

#include "base/bits.h"
#include "base/containers/span.h"
#include "base/sha1.h"
#include "base/strings/stringprintf.h"

extern char __executable_start;

namespace base {
namespace debug {

namespace {

#if __SIZEOF_POINTER__ == 4
using Ehdr = Elf32_Ehdr;
using Half = Elf32_Half;
using Nhdr = Elf32_Nhdr;
using Phdr = Elf32_Phdr;
#else
using Ehdr = Elf64_Ehdr;
using Half = Elf64_Half;
using Nhdr = Elf64_Nhdr;
using Phdr = Elf64_Phdr;
#endif

using ElfSegment = span<const char>;

Optional<std::string> ElfSegmentBuildIDNoteAsString(const ElfSegment& segment) {
  const void* section_end = segment.data() + segment.size_bytes();
  const Nhdr* note_header = reinterpret_cast<const Nhdr*>(segment.data());
  while (note_header < section_end) {
    if (note_header->n_type == NT_GNU_BUILD_ID)
      break;
    note_header = reinterpret_cast<const Nhdr*>(
        reinterpret_cast<const char*>(note_header) + sizeof(Nhdr) +
        bits::Align(note_header->n_namesz, 4) +
        bits::Align(note_header->n_descsz, 4));
  }

  if (note_header >= section_end || note_header->n_descsz != kSHA1Length)
    return nullopt;

  const uint8_t* guid = reinterpret_cast<const uint8_t*>(note_header) +
                        sizeof(Nhdr) + bits::Align(note_header->n_namesz, 4);

  uint32_t dword = htonl(*reinterpret_cast<const int32_t*>(guid));
  uint16_t word1 = htons(*reinterpret_cast<const int16_t*>(guid + 4));
  uint16_t word2 = htons(*reinterpret_cast<const int16_t*>(guid + 6));
  std::string identifier;
  identifier.reserve(kSHA1Length * 2);  // as hex string
  SStringPrintf(&identifier, "%08X%04X%04X", dword, word1, word2);
  for (size_t i = 8; i < note_header->n_descsz; ++i)
    StringAppendF(&identifier, "%02X", guid[i]);

  return identifier;
}

std::vector<ElfSegment> FindElfSegments(const char* elf_base,
                                        uint32_t segment_type) {
  if (strncmp(static_cast<const char*>(elf_base), ELFMAG, SELFMAG) != 0)
    return std::vector<ElfSegment>();

  const Ehdr* elf_header = reinterpret_cast<const Ehdr*>(elf_base);
  const Phdr* phdrs =
      reinterpret_cast<const Phdr*>(elf_base + elf_header->e_phoff);
  std::vector<ElfSegment> segments;
  for (Half i = 0; i < elf_header->e_phnum; ++i) {
    if (phdrs[i].p_type == segment_type)
      segments.push_back({elf_base + phdrs[i].p_offset, phdrs[i].p_filesz});
  }
  return segments;
}

}  // namespace

Optional<std::string> ReadElfBuildId() {
  // Elf program headers can have multiple PT_NOTE arrays.
  std::vector<ElfSegment> segs = FindElfSegments(&__executable_start, PT_NOTE);
  if (segs.empty())
    return nullopt;
  Optional<std::string> id;
  for (const ElfSegment& seg : segs) {
    id = ElfSegmentBuildIDNoteAsString(seg);
    if (id)
      return id;
  }

  return nullopt;
}

}  // namespace debug
}  // namespace base
