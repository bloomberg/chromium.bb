// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/cbor/cbor_writer.h"

#include "base/numerics/safe_conversions.h"

namespace content {

CBORWriter::~CBORWriter() {}

// static
std::vector<uint8_t> CBORWriter::Write(const CBORValue& node) {
  std::vector<uint8_t> cbor;
  CBORWriter writer(&cbor);
  writer.EncodeCBOR(node);
  return cbor;
}

CBORWriter::CBORWriter(std::vector<uint8_t>* cbor) : encoded_cbor_(cbor) {}

void CBORWriter::EncodeCBOR(const CBORValue& node) {
  switch (node.type()) {
    case CBORValue::Type::NONE: {
      StartItem(CborMajorType::kByteString, 0);
      return;
    }

    // Represents unsigned integers.
    case CBORValue::Type::UNSIGNED: {
      uint64_t value = node.GetUnsigned();
      StartItem(CborMajorType::kUnsigned, value);
      return;
    }

    // Represents a byte string.
    case CBORValue::Type::BYTESTRING: {
      const CBORValue::BinaryValue& bytes = node.GetBytestring();
      StartItem(CborMajorType::kByteString,
                base::strict_cast<uint64_t>(bytes.size()));
      // Add the bytes.
      encoded_cbor_->insert(encoded_cbor_->end(), bytes.begin(), bytes.end());
      return;
    }

    case CBORValue::Type::STRING: {
      base::StringPiece string = node.GetString();
      StartItem(CborMajorType::kString,
                base::strict_cast<uint64_t>(string.size()));

      // Add the characters.
      encoded_cbor_->insert(encoded_cbor_->end(), string.begin(), string.end());
      return;
    }

    // Represents an array.
    case CBORValue::Type::ARRAY: {
      const CBORValue::ArrayValue& array = node.GetArray();
      StartItem(CborMajorType::kArray, array.size());
      for (const auto& value : array) {
        EncodeCBOR(value);
      }
      return;
    }

    // Represents a map.
    case CBORValue::Type::MAP: {
      const CBORValue::MapValue& map = node.GetMap();
      StartItem(CborMajorType::kMap, map.size());
      for (const auto& value : map) {
        EncodeCBOR(CBORValue(value.first));
        EncodeCBOR(value.second);
      }
      return;
    }
  }
  NOTREACHED();
  return;
}

void CBORWriter::StartItem(CborMajorType type, uint64_t size) {
  encoded_cbor_->push_back(
      base::checked_cast<uint8_t>(static_cast<unsigned>(type) << 5));
  SetUint(size);
}

void CBORWriter::SetAdditionalInformation(uint8_t additional_information) {
  DCHECK(!encoded_cbor_->empty());
  DCHECK_EQ(additional_information & kAdditionalInformationDataMask,
            additional_information);
  encoded_cbor_->back() |=
      (additional_information & kAdditionalInformationDataMask);
}

void CBORWriter::SetUint(uint64_t value) {
  size_t count = GetNumUintBytes(value);
  int shift = -1;
  // Values under 24 are encoded directly in the initial byte.
  // Otherwise, the last 5 bits of the initial byte contains the length
  // of unsigned integer, which is encoded in following bytes.
  switch (count) {
    case 0:
      SetAdditionalInformation(base::checked_cast<uint8_t>(value));
      break;
    case 1:
      SetAdditionalInformation(kAdditionalInformation1Byte);
      shift = 0;
      break;
    case 2:
      SetAdditionalInformation(kAdditionalInformation2Bytes);
      shift = 1;
      break;
    case 4:
      SetAdditionalInformation(kAdditionalInformation4Bytes);
      shift = 3;
      break;
    case 8:
      SetAdditionalInformation(kAdditionalInformation8Bytes);
      shift = 7;
      break;
    default:
      NOTREACHED();
      break;
  }
  for (; shift >= 0; shift--) {
    encoded_cbor_->push_back(0xFF & (value >> (shift * 8)));
  }
}

size_t CBORWriter::GetNumUintBytes(uint64_t value) {
  if (value < 24) {
    return 0;
  } else if (value <= 0xFF) {
    return 1;
  } else if (value <= 0xFFFF) {
    return 2;
  } else if (value <= 0xFFFFFFFF) {
    return 4;
  }
  return 8;
}

}  // namespace content
