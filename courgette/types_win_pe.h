// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TYPES_WIN_PE_H_
#define TYPES_WIN_PE_H_

#include "base/basictypes.h"


namespace courgette {

// PE file section header.  This struct has the same layout as the
// IMAGE_SECTION_HEADER structure from WINNT.H
// http://msdn.microsoft.com/en-us/library/ms680341(VS.85).aspx
//
#pragma pack(push, 1)  // Supported by MSVC and GCC. Ensures no gaps in packing.
struct Section {
  char name[8];
  uint32 virtual_size;
  uint32 virtual_address;
  uint32 size_of_raw_data;
  uint32 file_offset_of_raw_data;
  uint32 pointer_to_relocations;   // Always zero in an image.
  uint32 pointer_to_line_numbers;  // Always zero in an image.
  uint16 number_of_relocations;    // Always zero in an image.
  uint16 number_of_line_numbers;   // Always zero in an image.
  uint32 characteristics;
};
#pragma pack(pop)

COMPILE_ASSERT(sizeof(Section) == 40, section_is_40_bytes);

// ImageDataDirectory has same layout as IMAGE_DATA_DIRECTORY structure from
// WINNT.H
// http://msdn.microsoft.com/en-us/library/ms680305(VS.85).aspx
//
class ImageDataDirectory {
 public:
  ImageDataDirectory() : address_(0), size_(0) {}
  RVA address_;
  uint32 size_;
};

COMPILE_ASSERT(sizeof(ImageDataDirectory) == 8,
               image_data_directory_is_8_bytes);


////////////////////////////////////////////////////////////////////////////////

// Constants and offsets gleaned from WINNT.H and various articles on the
// format of Windows PE executables.

// This is FIELD_OFFSET(IMAGE_DOS_HEADER, e_lfanew):
const size_t kOffsetOfFileAddressOfNewExeHeader = 0x3c;

const uint16 kImageNtOptionalHdr32Magic = 0x10b;
const uint16 kImageNtOptionalHdr64Magic = 0x20b;

const size_t kSizeOfCoffHeader = 20;
const size_t kOffsetOfDataDirectoryFromImageOptionalHeader32 = 96;
const size_t kOffsetOfDataDirectoryFromImageOptionalHeader64 = 112;

}  // namespace
#endif  // TYPES_WIN_PE_H_
