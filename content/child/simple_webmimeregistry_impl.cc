// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/simple_webmimeregistry_impl.h"

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mime_util/mime_util.h"
#include "media/base/mime_util.h"
#include "media/filters/stream_parser_factory.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebString;
using blink::WebMimeRegistry;

namespace content {

// static
std::string SimpleWebMimeRegistryImpl::ToASCIIOrEmpty(const WebString& string) {
  return string.containsOnlyASCII() ? string.ascii() : std::string();
}

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsMIMEType(
    const WebString& mime_type) {
  return mime_util::IsSupportedMimeType(ToASCIIOrEmpty(mime_type))
             ? WebMimeRegistry::IsSupported
             : WebMimeRegistry::IsNotSupported;
}

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsImageMIMEType(
    const WebString& mime_type) {
  return mime_util::IsSupportedImageMimeType(ToASCIIOrEmpty(mime_type))
             ? WebMimeRegistry::IsSupported
             : WebMimeRegistry::IsNotSupported;
}

WebMimeRegistry::SupportsType
    SimpleWebMimeRegistryImpl::supportsImagePrefixedMIMEType(
    const WebString& mime_type) {
    std::string ascii_mime_type = ToASCIIOrEmpty(mime_type);
    return (mime_util::IsSupportedImageMimeType(ascii_mime_type) ||
            (base::StartsWith(ascii_mime_type, "image/",
                              base::CompareCase::SENSITIVE) &&
             mime_util::IsSupportedNonImageMimeType(ascii_mime_type)))
               ? WebMimeRegistry::IsSupported
               : WebMimeRegistry::IsNotSupported;
}

WebMimeRegistry::SupportsType
    SimpleWebMimeRegistryImpl::supportsJavaScriptMIMEType(
    const WebString& mime_type) {
  return mime_util::IsSupportedJavascriptMimeType(ToASCIIOrEmpty(mime_type))
             ? WebMimeRegistry::IsSupported
             : WebMimeRegistry::IsNotSupported;
}

WebMimeRegistry::SupportsType SimpleWebMimeRegistryImpl::supportsMediaMIMEType(
    const WebString& mime_type,
    const WebString& codecs) {
  const std::string mime_type_ascii = ToASCIIOrEmpty(mime_type);
  std::vector<std::string> codec_vector;
  media::ParseCodecString(ToASCIIOrEmpty(codecs), &codec_vector, false);
  return static_cast<WebMimeRegistry::SupportsType>(
      media::IsSupportedMediaFormat(mime_type_ascii, codec_vector));
}

bool SimpleWebMimeRegistryImpl::supportsMediaSourceMIMEType(
    const WebString& mime_type,
    const WebString& codecs) {
  const std::string mime_type_ascii = ToASCIIOrEmpty(mime_type);
  if (mime_type_ascii.empty())
    return false;
  std::vector<std::string> parsed_codec_ids;
  media::ParseCodecString(ToASCIIOrEmpty(codecs), &parsed_codec_ids, false);
  return media::StreamParserFactory::IsTypeSupported(mime_type_ascii,
                                                     parsed_codec_ids);
}

WebMimeRegistry::SupportsType
    SimpleWebMimeRegistryImpl::supportsNonImageMIMEType(
    const WebString& mime_type) {
  return mime_util::IsSupportedNonImageMimeType(ToASCIIOrEmpty(mime_type))
             ? WebMimeRegistry::IsSupported
             : WebMimeRegistry::IsNotSupported;
}

WebString SimpleWebMimeRegistryImpl::mimeTypeForExtension(
    const WebString& file_extension) {
  std::string mime_type;
  net::GetMimeTypeFromExtension(
      blink::WebStringToFilePath(file_extension).value(), &mime_type);
  return WebString::fromUTF8(mime_type);
}

WebString SimpleWebMimeRegistryImpl::wellKnownMimeTypeForExtension(
    const WebString& file_extension) {
  std::string mime_type;
  net::GetWellKnownMimeTypeFromExtension(
      blink::WebStringToFilePath(file_extension).value(), &mime_type);
  return WebString::fromUTF8(mime_type);
}

}  // namespace content
