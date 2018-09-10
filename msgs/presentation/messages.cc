// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "msgs/presentation/messages.h"

#include <cstring>

#include "platform/api/logging.h"
#include "third_party/tinycbor/src/src/cbor.h"
#include "third_party/tinycbor/src/src/utf8_p.h"

namespace openscreen {
namespace msgs {
namespace presentation {
namespace {

constexpr char kRequestIdKey[] = "request-id";
constexpr char kUrlsKey[] = "urls";

#define CBOR_RETURN_WHAT_ON_ERROR(stmt, what)                           \
  {                                                                     \
    CborError error = stmt;                                             \
    /* Encoder-specific errors, so it's fine to check these even in the \
     * parser.                                                          \
     */                                                                 \
    DCHECK_NE(error, CborErrorTooFewItems);                             \
    DCHECK_NE(error, CborErrorTooManyItems);                            \
    DCHECK_NE(error, CborErrorDataTooLarge);                            \
    if (error != CborNoError && error != CborErrorOutOfMemory) {        \
      return what;                                                      \
    }                                                                   \
  }
#define CBOR_RETURN_ON_ERROR_INTERNAL(stmt) \
  CBOR_RETURN_WHAT_ON_ERROR(stmt, error)
#define CBOR_RETURN_ON_ERROR(stmt) CBOR_RETURN_WHAT_ON_ERROR(stmt, -error)

#define EXPECT_KEY_CONSTANT(it, key) ExpectKey(it, key, sizeof(key) - 1)

CborError IsValidUtf8(const std::string& s) {
  const uint8_t* buffer = reinterpret_cast<const uint8_t*>(s.data());
  const uint8_t* end = buffer + s.size();
  while (buffer < end) {
    // TODO(btolsch): This is an implementation detail of tinycbor so we should
    // eventually replace this call with our own utf8 validation.
    if (get_utf8(&buffer, end) == ~0u) {
      return CborErrorInvalidUtf8TextString;
    }
  }
  return CborNoError;
}

CborError ExpectKey(CborValue* it, const char* key, size_t key_length) {
  size_t observed_length = 0;
  CBOR_RETURN_ON_ERROR_INTERNAL(
      cbor_value_get_string_length(it, &observed_length));
  if (observed_length != key_length) {
    return CborErrorImproperValue;
  }
  std::string observed_key(key_length, 0);
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_copy_text_string(
      it, const_cast<char*>(observed_key.data()), &observed_length, nullptr));
  if (observed_key != key) {
    return CborErrorImproperValue;
  }
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_advance(it));
  return CborNoError;
}

CborError ExtractStringArray(CborValue* it,
                             std::vector<std::string>* strings_out) {
  CborValue array;
  size_t array_length = 0;
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_get_array_length(it, &array_length));
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_enter_container(it, &array));
  strings_out->reserve(array_length);
  for (size_t i = 0; i < array_length; ++i) {
    size_t url_length = 0;
    CBOR_RETURN_ON_ERROR_INTERNAL(
        cbor_value_validate(&array, CborValidateUtf8));
    if (cbor_value_is_length_known(&array)) {
      CBOR_RETURN_ON_ERROR_INTERNAL(
          cbor_value_get_string_length(&array, &url_length));
    } else {
      CBOR_RETURN_ON_ERROR_INTERNAL(
          cbor_value_calculate_string_length(&array, &url_length));
    }
    strings_out->emplace_back(url_length, 0);
    CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_copy_text_string(
        &array, const_cast<char*>(strings_out->back().data()), &url_length,
        nullptr));
    CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_advance(&array));
  }
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_leave_container(it, &array));
  return CborNoError;
}

}  // namespace

ssize_t EncodeUrlAvailabilityRequest(const UrlAvailabilityRequest& request,
                                     uint8_t* buffer,
                                     size_t length) {
  CborEncoder encoder;
  CborEncoder map;
  cbor_encoder_init(&encoder, buffer, length, 0);
  CBOR_RETURN_ON_ERROR(cbor_encode_tag(
      &encoder, static_cast<uint64_t>(Tag::kUrlAvailabilityRequest)));
  CBOR_RETURN_ON_ERROR(cbor_encoder_create_map(&encoder, &map, 2));

  CBOR_RETURN_ON_ERROR(
      cbor_encode_text_string(&map, kRequestIdKey, sizeof(kRequestIdKey) - 1));
  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&map, request.request_id));

  // TODO(btolsch): Writing an array of strings to a map key should be a
  // separate function once we need to do it again.
  CborEncoder array;
  CBOR_RETURN_ON_ERROR(
      cbor_encode_text_string(&map, kUrlsKey, sizeof(kUrlsKey) - 1));
  CBOR_RETURN_ON_ERROR(
      cbor_encoder_create_array(&map, &array, request.urls.size()));
  for (const auto& url : request.urls) {
    CBOR_RETURN_ON_ERROR(IsValidUtf8(url));
    CBOR_RETURN_ON_ERROR(
        cbor_encode_text_string(&array, url.c_str(), url.size()));
  }
  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&map, &array));

  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder, &map));

  size_t extra_bytes_needed = cbor_encoder_get_extra_bytes_needed(&encoder);
  if (extra_bytes_needed) {
    return static_cast<ssize_t>(length + extra_bytes_needed);
  } else {
    return static_cast<ssize_t>(cbor_encoder_get_buffer_size(&encoder, buffer));
  }
}

ssize_t DecodeUrlAvailabilityRequest(uint8_t* buffer,
                                     size_t length,
                                     UrlAvailabilityRequest* request) {
  CborParser parser;
  CborValue it;

  CBOR_RETURN_ON_ERROR(cbor_parser_init(buffer, length, 0, &parser, &it));
  // TODO(btolsch): In the future, we will read the tag first and switch on it
  // to choose a parsing function.
  if (cbor_value_get_type(&it) != CborTagType) {
    return -1;
  }
  uint64_t tag = 0;
  cbor_value_get_tag(&it, &tag);
  if (tag != static_cast<uint64_t>(Tag::kUrlAvailabilityRequest)) {
    return -1;
  }
  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it));
  if (cbor_value_get_type(&it) != CborMapType) {
    return -1;
  }

  CborValue map;
  size_t map_length = 0;
  CBOR_RETURN_ON_ERROR(cbor_value_get_map_length(&it, &map_length));
  if (map_length != 2) {
    return -1;
  }
  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&it, &map));
  CBOR_RETURN_ON_ERROR(EXPECT_KEY_CONSTANT(&map, kRequestIdKey));
  CBOR_RETURN_ON_ERROR(cbor_value_get_uint64(&map, &request->request_id));
  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&map));

  CBOR_RETURN_ON_ERROR(EXPECT_KEY_CONSTANT(&map, kUrlsKey));

  CBOR_RETURN_ON_ERROR(ExtractStringArray(&map, &request->urls));
  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&it, &map));

  auto result = static_cast<ssize_t>(cbor_value_get_next_byte(&it) - buffer);
  return result;
}

}  // namespace presentation
}  // namespace msgs
}  // namespace openscreen
