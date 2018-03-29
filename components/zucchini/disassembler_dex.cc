// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/disassembler_dex.h"

#include <cmath>
#include <set>
#include <utility>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/zucchini/buffer_source.h"
#include "components/zucchini/buffer_view.h"

namespace zucchini {

namespace {

// Size of a Dalvik instruction unit. Need to cast to signed int because
// sizeof() gives size_t, which dominates when operated on ptrdiff_t, then
// wrecks havoc for base::checked_cast<int16_t>().
constexpr int kInstrUnitSize = static_cast<int>(sizeof(uint16_t));

// Buffer for ReadDexHeader() to optionally return results.
struct ReadDexHeaderResults {
  BufferSource source;
  const dex::HeaderItem* header;
  int dex_version;
};

// Returns whether |image| points to a DEX file. If this is a possibility and
// |opt_results| is not null, then uses it to pass extracted data to enable
// further parsing.
bool ReadDexHeader(ConstBufferView image, ReadDexHeaderResults* opt_results) {
  // This part needs to be fairly efficient since it may be called many times.
  BufferSource source(image);
  const dex::HeaderItem* header = source.GetPointer<dex::HeaderItem>();
  if (!header)
    return false;
  if (header->magic[0] != 'd' || header->magic[1] != 'e' ||
      header->magic[2] != 'x' || header->magic[3] != '\n' ||
      header->magic[7] != '\0') {
    return false;
  }

  // Magic matches: More detailed tests can be conducted.
  int dex_version = 0;
  for (int i = 4; i < 7; ++i) {
    if (!isdigit(header->magic[i]))
      return false;
    dex_version = dex_version * 10 + (header->magic[i] - '0');
  }
  if (dex_version != 35 && dex_version != 37)
    return false;

  if (header->file_size > image.size() ||
      header->file_size < sizeof(dex::HeaderItem) ||
      header->map_off < sizeof(dex::HeaderItem)) {
    return false;
  }

  if (opt_results)
    *opt_results = {source, header, dex_version};
  return true;
}

}  // namespace

/******** CodeItemParser ********/

// A parser to extract successive code items from a DEX image whose header has
// been parsed.
class CodeItemParser {
 public:
  using size_type = BufferSource::size_type;

  explicit CodeItemParser(ConstBufferView image) : image_(image) {}

  // Initializes the parser, returns true on success and false on error.
  bool Init(const dex::MapItem& code_map_item) {
    // Sanity check to quickly fail if |code_map_item.offset| or
    // |code_map_item.size| is too large. This is a heuristic because code item
    // sizes need to be parsed (sizeof(dex::CodeItem) is a lower bound).
    if (!image_.covers_array(code_map_item.offset, code_map_item.size,
                             sizeof(dex::CodeItem))) {
      return false;
    }
    source_ = std::move(BufferSource(image_).Skip(code_map_item.offset));
    return true;
  }

  // Extracts the header of the next code item, and skips the variable-length
  // data. Returns the offset of the code item if successful. Otherwise returns
  // kInvalidOffset, and thereafter the parser becomes valid. For reference,
  // here's a pseudo-struct of a complete code item:
  //
  // struct code_item {
  //   // 4-byte aligned here.
  //   // 16-byte header defined (dex::CodeItem).
  //   uint16_t registers_size;
  //   uint16_t ins_size;
  //   uint16_t outs_size;
  //   uint16_t tries_size;
  //   uint32_t debug_info_off;
  //   uint32_t insns_size;
  //
  //   // Variable-length data follow.
  //   uint16_t insns[insns_size];  // Instruction bytes.
  //   uint16_t padding[(tries_size > 0 && insns_size % 2 == 1) ? 1 : 0];
  //
  //   if (tries_size > 0) {
  //     // 4-byte aligned here.
  //     struct try_item {  // dex::TryItem.
  //       uint32_t start_addr;
  //       uint16_t insn_count;
  //       uint16_t handler_off;
  //     } tries[tries_size];
  //
  //     struct encoded_catch_handler_list {
  //       uleb128 handlers_size;
  //       struct encoded_catch_handler {
  //         sleb128 encoded_catch_handler_size;
  //         struct encoded_type_addr_pair {
  //           uleb128 type_idx;
  //           uleb128 addr;
  //         } handlers[abs(encoded_catch_handler_size)];
  //         if (encoded_catch_handler_size <= 0) {
  //           uleb128 catch_all_addr;
  //         }
  //       } handlers_list[handlers_size];
  //     } handlers_group;  // Confusingly called "handlers" in DEX doc.
  //   }
  //
  //   // Padding to 4-bytes align next code_item *only if more exist*.
  // }
  offset_t GetNext() {
    // Read header CodeItem.
    if (!source_.AlignOn(image_, 4U))
      return kInvalidOffset;
    const offset_t code_item_offset =
        base::checked_cast<offset_t>(source_.begin() - image_.begin());
    const auto* code_item = source_.GetPointer<const dex::CodeItem>();
    if (!code_item)
      return kInvalidOffset;
    DCHECK_EQ(0U, code_item_offset % 4U);

    // Skip instruction bytes.
    if (!source_.GetArray<uint16_t>(code_item->insns_size))
      return kInvalidOffset;
    // Skip padding if present.
    if (code_item->tries_size > 0 && !source_.AlignOn(image_, 4U))
      return kInvalidOffset;

    // Skip tries[] and handlers_group to arrive at the next code item. Parsing
    // is nontrivial due to use of uleb128 / sleb128.
    if (code_item->tries_size > 0) {
      // Skip (try_item) tries[].
      if (!source_.GetArray<dex::TryItem>(code_item->tries_size))
        return kInvalidOffset;

      // Skip handlers_group.
      uint32_t handlers_size = 0;
      if (!source_.GetUleb128(&handlers_size))
        return kInvalidOffset;
      // Sanity check to quickly reject excessively large |handlers_size|.
      if (source_.Remaining() < static_cast<size_type>(handlers_size))
        return kInvalidOffset;

      // Skip (encoded_catch_handler) handlers_list[].
      for (uint32_t k = 0; k < handlers_size; ++k) {
        int32_t encoded_catch_handler_size = 0;
        if (!source_.GetSleb128(&encoded_catch_handler_size))
          return kInvalidOffset;
        const size_type abs_size = std::abs(encoded_catch_handler_size);
        if (source_.Remaining() < abs_size)  // Sanity check.
          return kInvalidOffset;
        // Skip (encoded_type_addr_pair) handlers[].
        for (size_type j = 0; j < abs_size; ++j) {
          if (!source_.SkipLeb128() || !source_.SkipLeb128())
            return kInvalidOffset;
        }
        // Skip catch_all_addr.
        if (encoded_catch_handler_size <= 0) {
          if (!source_.SkipLeb128())
            return kInvalidOffset;
        }
      }
    }
    // Success! |code_item->insns_size| is validated, but its content is still
    // considered unsafe and requires validation.
    return code_item_offset;
  }

  // Given |code_item_offset| that points to the start of a valid code item in
  // |image|, returns |insns| bytes as ConstBufferView.
  static ConstBufferView GetCodeItemInsns(ConstBufferView image,
                                          offset_t code_item_offset) {
    BufferSource source(BufferSource(image).Skip(code_item_offset));
    const auto* code_item = source.GetPointer<const dex::CodeItem>();
    DCHECK(code_item);
    BufferRegion insns{0, code_item->insns_size * kInstrUnitSize};
    DCHECK(source.covers(insns));
    return source[insns];
  }

 private:
  ConstBufferView image_;
  BufferSource source_;
};

/******** DisassemblerDex ********/

DisassemblerDex::DisassemblerDex() : Disassembler(4) {}

DisassemblerDex::~DisassemblerDex() = default;

// static.
bool DisassemblerDex::QuickDetect(ConstBufferView image) {
  return ReadDexHeader(image, nullptr);
}

ExecutableType DisassemblerDex::GetExeType() const {
  return kExeTypeDex;
}

std::string DisassemblerDex::GetExeTypeString() const {
  return base::StringPrintf("DEX (version %d)", dex_version_);
}

std::vector<ReferenceGroup> DisassemblerDex::MakeReferenceGroups() const {
  return {};
}

bool DisassemblerDex::Parse(ConstBufferView image) {
  image_ = image;
  return ParseHeader();
}

bool DisassemblerDex::ParseHeader() {
  ReadDexHeaderResults results;
  if (!ReadDexHeader(image_, &results))
    return false;

  header_ = results.header;
  dex_version_ = results.dex_version;
  BufferSource source = results.source;

  // DEX header contains file size, so use it to resize |image_| right away.
  image_.shrink(header_->file_size);

  // Read map list. This is not a fixed-size array, so instead of reading
  // MapList directly, read |MapList::size| first, then visit elements in
  // |MapList::list|.
  static_assert(
      offsetof(dex::MapList, list) == sizeof(decltype(dex::MapList::size)),
      "MapList size error.");
  source = std::move(BufferSource(image_).Skip(header_->map_off));
  decltype(dex::MapList::size) list_size = 0;
  if (!source.GetValue(&list_size) || list_size > dex::kMaxItemListSize)
    return false;
  const auto* item_list = source.GetArray<const dex::MapItem>(list_size);
  if (!item_list)
    return false;

  // Read and validate map list, ensuring that required item types are present.
  std::set<uint16_t> required_item_types = {
      dex::kTypeStringIdItem, dex::kTypeTypeIdItem, dex::kTypeFieldIdItem,
      dex::kTypeMethodIdItem, dex::kTypeCodeItem};
  for (offset_t i = 0; i < list_size; ++i) {
    const dex::MapItem* item = &item_list[i];
    // Sanity check to reject unreasonably large |item->size|.
    // TODO(huangs): Implement a more stringent check.
    if (!image_.covers({item->offset, item->size}))
      return false;
    if (!map_item_map_.insert(std::make_pair(item->type, item)).second)
      return false;  // A given type must appear at most once.
    required_item_types.erase(item->type);
  }
  if (!required_item_types.empty())
    return false;

  // Make local copies of main map items.
  string_map_item_ = *map_item_map_[dex::kTypeStringIdItem];
  type_map_item_ = *map_item_map_[dex::kTypeTypeIdItem];
  field_map_item_ = *map_item_map_[dex::kTypeFieldIdItem];
  method_map_item_ = *map_item_map_[dex::kTypeMethodIdItem];
  code_map_item_ = *map_item_map_[dex::kTypeCodeItem];

  // Iteratively extract variable-length code items blocks. Any failure would
  // indicate invalid DEX. Success indicates that no structural problem is
  // found. However, contained instructions still need validation on use.
  CodeItemParser code_item_parser(image_);
  if (!code_item_parser.Init(code_map_item_))
    return false;
  code_item_offsets_.resize(code_map_item_.size);
  for (size_t i = 0; i < code_map_item_.size; ++i) {
    const offset_t code_item_offset = code_item_parser.GetNext();
    if (code_item_offset == kInvalidOffset)
      return false;
    code_item_offsets_[i] = code_item_offset;
  }
  return true;
}

}  // namespace zucchini
