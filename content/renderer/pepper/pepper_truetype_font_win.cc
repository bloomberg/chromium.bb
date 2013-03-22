// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_truetype_font.h"

#include <windows.h>
#include <set>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_byteorder.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "ppapi/c/dev/ppb_truetype_font_dev.h"
#include "ppapi/c/pp_errors.h"

namespace content {

namespace {

class PepperTrueTypeFontWin : public PepperTrueTypeFont {
 public:
  explicit PepperTrueTypeFontWin(
      const ppapi::proxy::SerializedTrueTypeFontDesc& desc);
  virtual ~PepperTrueTypeFontWin();

  // PepperTrueTypeFont overrides.
  virtual bool IsValid() OVERRIDE;
  virtual int32_t Describe(
      ppapi::proxy::SerializedTrueTypeFontDesc* desc) OVERRIDE;
  virtual int32_t GetTableTags(std::vector<uint32_t>* tags) OVERRIDE;
  virtual int32_t GetTable(uint32_t table_tag,
                           int32_t offset,
                           int32_t max_data_length,
                           std::string* data) OVERRIDE;
 private:
  HFONT font_;

  DISALLOW_COPY_AND_ASSIGN(PepperTrueTypeFontWin);
};

PepperTrueTypeFontWin::PepperTrueTypeFontWin(
    const ppapi::proxy::SerializedTrueTypeFontDesc& desc) {
  DWORD pitch_and_family = DEFAULT_PITCH;
  switch (desc.generic_family) {
    case PP_TRUETYPEFONTFAMILY_SERIF:
      pitch_and_family |= FF_ROMAN;
      break;
    case PP_TRUETYPEFONTFAMILY_SANSSERIF:
      pitch_and_family |= FF_SWISS;
      break;
    case PP_TRUETYPEFONTFAMILY_CURSIVE:
      pitch_and_family |= FF_SCRIPT;
      break;
    case PP_TRUETYPEFONTFAMILY_FANTASY:
      pitch_and_family |= FF_DECORATIVE;
      break;
    case PP_TRUETYPEFONTFAMILY_MONOSPACE:
      pitch_and_family |= FF_MODERN;
      break;
  }
  // TODO weight, variant.

  font_ = CreateFont(0  /* height */,
                     0  /* width */,
                     0  /* escapement */,
                     0  /* orientation */,
                     desc.weight,  // our weight enum matches Windows.
                     (desc.style & PP_TRUETYPEFONTSTYLE_ITALIC) ? 1 : 0,
                     0  /* underline */,
                     0  /* strikeout */,
                     desc.charset,  // our charset enum matches Windows.
                     OUT_OUTLINE_PRECIS  /* truetype and other outline fonts */,
                     CLIP_DEFAULT_PRECIS,
                     DEFAULT_QUALITY,
                     pitch_and_family,
                     UTF8ToUTF16(desc.family).c_str());
}

PepperTrueTypeFontWin::~PepperTrueTypeFontWin() {
}

bool PepperTrueTypeFontWin::IsValid() {
  return font_ != NULL;
}

int32_t PepperTrueTypeFontWin::Describe(
      ppapi::proxy::SerializedTrueTypeFontDesc* desc) {
  LOGFONT font_desc;
  if (!::GetObject(font_, sizeof(LOGFONT), &font_desc))
    return PP_ERROR_FAILED;

  switch (font_desc.lfPitchAndFamily & 0xF0) {  // Top 4 bits are family.
    case FF_ROMAN:
      desc->generic_family = PP_TRUETYPEFONTFAMILY_SERIF;
      break;
    case FF_SWISS:
      desc->generic_family = PP_TRUETYPEFONTFAMILY_SANSSERIF;
      break;
    case FF_SCRIPT:
      desc->generic_family = PP_TRUETYPEFONTFAMILY_CURSIVE;
      break;
    case FF_DECORATIVE:
      desc->generic_family = PP_TRUETYPEFONTFAMILY_FANTASY;
      break;
    case FF_MODERN:
      desc->generic_family = PP_TRUETYPEFONTFAMILY_MONOSPACE;
      break;
  }

  desc->style = font_desc.lfItalic ? PP_TRUETYPEFONTSTYLE_ITALIC :
                                     PP_TRUETYPEFONTSTYLE_NORMAL;
  desc->weight = static_cast<PP_TrueTypeFontWeight_Dev>(font_desc.lfWeight);
  desc->width = PP_TRUETYPEFONTWIDTH_NORMAL;
  desc->charset =
      static_cast<PP_TrueTypeFontCharset_Dev>(font_desc.lfCharSet);

  // To get the face name, select the font and query for the name. GetObject
  // doesn't fill in the name field of the LOGFONT structure.
  base::win::ScopedCreateDC hdc(::CreateCompatibleDC(NULL));
  if (hdc) {
    base::win::ScopedSelectObject select_object(hdc, font_);
    WCHAR name[LF_FACESIZE];
    GetTextFace(hdc, LF_FACESIZE, name);
    desc->family = UTF16ToUTF8(name);
  }
  return PP_OK;
}

int32_t PepperTrueTypeFontWin::GetTableTags(std::vector<uint32_t>* tags) {
  base::win::ScopedCreateDC hdc(::CreateCompatibleDC(NULL));
  if (!hdc)
    return PP_ERROR_FAILED;

  base::win::ScopedSelectObject select_object(hdc, font_);

  // Get the 2 byte numTables field at an offset of 4 in the font. We need to
  // read at least 4 bytes or this fails.
  uint8_t num_tables_buf[4];
  if (::GetFontData(hdc, 0, 4, num_tables_buf, 4) != 4)
    return PP_ERROR_FAILED;
  // Font data is stored in big-endian order.
  DWORD num_tables = (num_tables_buf[0] << 8) | num_tables_buf[1];

  // The size in bytes of an entry in the table directory.
  static const DWORD kFontHeaderSize = 12;
  static const DWORD kTableEntrySize = 16;
  DWORD table_size = num_tables * kTableEntrySize;
  scoped_array<uint8_t> table_entries(new uint8_t[table_size]);
  // Get the table directory entries starting at an offset of 12 in the font.
  if (::GetFontData(hdc, 0 /* tag */, kFontHeaderSize,
                    table_entries.get(),
                    table_size) != table_size)
    return PP_ERROR_FAILED;

  tags->resize(num_tables);
  for (DWORD i = 0; i < num_tables; i++) {
    const uint8_t* entry = table_entries.get() + i * kTableEntrySize;
    uint32_t tag = static_cast<uint32_t>(entry[0]) << 24 |
                   static_cast<uint32_t>(entry[1]) << 16 |
                   static_cast<uint32_t>(entry[2]) << 8  |
                   static_cast<uint32_t>(entry[3]);
    (*tags)[i] = tag;
  }

  return num_tables;
}

int32_t PepperTrueTypeFontWin::GetTable(uint32_t table_tag,
                                        int32_t offset,
                                        int32_t max_data_length,
                                        std::string* data) {
  base::win::ScopedCreateDC hdc(::CreateCompatibleDC(NULL));
  if (!hdc)
    return PP_ERROR_FAILED;

  base::win::ScopedSelectObject select_object(hdc, font_);

  // Tags are byte swapped on Windows.
  table_tag = base::ByteSwap(table_tag);
  // Get the size of the font table first.
  DWORD table_size = ::GetFontData(hdc, table_tag, 0, NULL, 0);
  if (table_size == GDI_ERROR)
    return PP_ERROR_FAILED;

  DWORD safe_offset = std::min(static_cast<DWORD>(offset), table_size);
  DWORD safe_length = std::min(table_size - safe_offset,
                               static_cast<DWORD>(max_data_length));
  data->resize(safe_length);
  ::GetFontData(hdc, table_tag, safe_offset,
                reinterpret_cast<uint8_t*>(&(*data)[0]), safe_length);
  return static_cast<int32_t>(safe_length);
}

}  // namespace

// static
PepperTrueTypeFont* PepperTrueTypeFont::Create(
    const ppapi::proxy::SerializedTrueTypeFontDesc& desc) {
  return new PepperTrueTypeFontWin(desc);
}

}  // namespace content
