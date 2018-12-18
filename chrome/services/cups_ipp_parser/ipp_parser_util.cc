// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_ipp_parser/ipp_parser_util.h"

#include <cups/cups.h>
#include <memory>
#include <utility>

#include "base/optional.h"

namespace chrome {
namespace {

// Converts |value_tag| to corresponding mojom type for marshalling.
base::Optional<mojom::ValueType> ValueTagToType(const int value_tag) {
  switch (value_tag) {
    case IPP_TAG_BOOLEAN:
      return mojom::ValueType::BOOLEAN;
    case IPP_TAG_DATE:
      return mojom::ValueType::DATE;
    case IPP_TAG_INTEGER:
      return mojom::ValueType::INTEGER;

    // Below string cases taken from libCUPS ippAttributeString API
    case IPP_TAG_TEXT:
    case IPP_TAG_NAME:
    case IPP_TAG_KEYWORD:
    case IPP_TAG_CHARSET:
    case IPP_TAG_URI:
    case IPP_TAG_URISCHEME:
    case IPP_TAG_MIMETYPE:
    case IPP_TAG_LANGUAGE:
    case IPP_TAG_TEXTLANG:
    case IPP_TAG_NAMELANG:
      return mojom::ValueType::STRING;

    default:
      NOTREACHED();
  }

  return base::nullopt;
}

// Callback used with ippReadIO (libcups API)
// Repeatedly used to copy IPP request string -> ipp_t.
ssize_t IppRead(base::StringPiece* src, ipp_uchar_t* dst, size_t bytes) {
  size_t num_to_write = std::min(src->size(), bytes);
  std::copy(src->begin(), src->begin() + num_to_write, dst);
  src->remove_prefix(num_to_write);

  return num_to_write;
}

}  // namespace

// Parses and converts |ipp| to corresponding mojom type for marshalling.
mojom::IppMessagePtr ParseIppMessage(ipp_t* ipp) {
  mojom::IppMessagePtr ret = mojom::IppMessage::New();

  // Parse version numbers
  int major, minor;
  major = ippGetVersion(ipp, &minor);
  ret->major_version = major;
  ret->minor_version = minor;

  // IPP request opcode ids are specified by the RFC, so casting to ints is
  // safe.
  ret->operation_id = static_cast<int>(ippGetOperation(ipp));
  if (!ret->operation_id) {
    return nullptr;
  }

  // Parse request id
  ret->request_id = ippGetRequestId(ipp);
  if (!ret->request_id) {
    return nullptr;
  }

  std::vector<mojom::IppAttributePtr> attributes;
  for (ipp_attribute_t* attr = ippFirstAttribute(ipp); attr != NULL;
       attr = ippNextAttribute(ipp)) {
    mojom::IppAttributePtr attrptr = mojom::IppAttribute::New();

    attrptr->name = ippGetName(attr);
    attrptr->group_tag = ippGetGroupTag(attr);
    if (attrptr->group_tag == IPP_TAG_ZERO) {
      return nullptr;
    }

    attrptr->value_tag = ippGetValueTag(attr);
    if (attrptr->value_tag == IPP_TAG_ZERO) {
      return nullptr;
    }

    auto type = ValueTagToType(attrptr->value_tag);
    if (!type) {
      return nullptr;
    }
    attrptr->type = type.value();

    std::vector<mojom::ValuePtr> values;
    for (int i = 0; i < ippGetCount(attr); ++i) {
      auto value = mojom::Value::New();
      switch (attrptr->type) {
        case mojom::ValueType::BOOLEAN: {
          auto v = ippGetBoolean(attr, i);
          if (!v) {
            return nullptr;
          }
          value->set_bool_value(v);
          break;
        }
        case mojom::ValueType::DATE: {
          auto* v = ippGetDate(attr, i);
          if (!v) {
            return nullptr;
          }
          value->set_char_value(*v);
          break;
        }
        case mojom::ValueType::INTEGER: {
          auto v = ippGetInteger(attr, i);
          if (!v) {
            return nullptr;
          }
          value->set_int_value(v);
          break;
        }
        case mojom::ValueType::STRING: {
          auto* v = ippGetString(
              attr, i, NULL /* TODO(crbug/781061): figure out language */);
          if (!v) {
            return nullptr;
          }
          value->set_string_value(v);
          break;
        }
        default:
          NOTREACHED();
      }
      values.emplace_back(std::move(value));
    }
    attrptr->values = std::move(values);

    attributes.emplace_back(std::move(attrptr));
  }

  ret->attributes = std::move(attributes);
  return ret;
}

// Synchronously reads/parses |ipp_slice| and returns the resulting ipp_t
// object.
printing::ScopedIppPtr ReadIppSlice(base::StringPiece ipp_slice) {
  printing::ScopedIppPtr ipp = printing::WrapIpp(ippNew());

  // Casting above callback function to correct internal CUPS type
  // Note: This is safe since we essentially only cast the first argument
  // from base::StringPiece* --> void* and only access it from the former.
  auto ret = ippReadIO(&ipp_slice, reinterpret_cast<ipp_iocb_t>(IppRead), 1,
                       nullptr, ipp.get());

  if (ret == IPP_STATE_ERROR) {
    // Read failed, clear and return nullptr
    ipp.reset(nullptr);
  }

  return ipp;
}

}  // namespace chrome
