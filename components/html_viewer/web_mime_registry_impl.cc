// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_mime_registry_impl.h"

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mime_util/mime_util.h"
#include "media/base/key_systems.h"
#include "media/base/mime_util.h"
#include "media/filters/stream_parser_factory.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace html_viewer {
namespace {

std::string ToASCIIOrEmpty(const blink::WebString& string) {
  return base::IsStringASCII(string)
      ? base::UTF16ToASCII(base::StringPiece16(string))
      : std::string();
}

}  // namespace

blink::WebMimeRegistry::SupportsType WebMimeRegistryImpl::supportsMIMEType(
    const blink::WebString& mime_type) {
  return mime_util::IsSupportedMimeType(ToASCIIOrEmpty(mime_type))
             ? blink::WebMimeRegistry::IsSupported
             : blink::WebMimeRegistry::IsNotSupported;
}

blink::WebMimeRegistry::SupportsType WebMimeRegistryImpl::supportsImageMIMEType(
    const blink::WebString& mime_type) {
  return mime_util::IsSupportedImageMimeType(ToASCIIOrEmpty(mime_type))
             ? blink::WebMimeRegistry::IsSupported
             : blink::WebMimeRegistry::IsNotSupported;
}

blink::WebMimeRegistry::SupportsType
WebMimeRegistryImpl::supportsImagePrefixedMIMEType(
    const blink::WebString& mime_type) {
  std::string ascii_mime_type = ToASCIIOrEmpty(mime_type);
  return (mime_util::IsSupportedImageMimeType(ascii_mime_type) ||
          (base::StartsWith(ascii_mime_type, "image/",
                            base::CompareCase::SENSITIVE) &&
           mime_util::IsSupportedNonImageMimeType(ascii_mime_type)))
             ? WebMimeRegistry::IsSupported
             : WebMimeRegistry::IsNotSupported;
}

blink::WebMimeRegistry::SupportsType
    WebMimeRegistryImpl::supportsJavaScriptMIMEType(
    const blink::WebString& mime_type) {
  return mime_util::IsSupportedJavascriptMimeType(ToASCIIOrEmpty(mime_type))
             ? blink::WebMimeRegistry::IsSupported
             : blink::WebMimeRegistry::IsNotSupported;
}

blink::WebMimeRegistry::SupportsType WebMimeRegistryImpl::supportsMediaMIMEType(
    const blink::WebString& mime_type,
    const blink::WebString& codecs,
    const blink::WebString& key_system) {
  const std::string mime_type_ascii = ToASCIIOrEmpty(mime_type);
  // Not supporting the container is a flat-out no.
  if (!media::IsSupportedMediaMimeType(mime_type_ascii))
    return IsNotSupported;

  // Mojo does not currently support any key systems.
  if (!key_system.isEmpty())
    return IsNotSupported;

  // Check list of strict codecs to see if it is supported.
  if (media::IsStrictMediaMimeType(mime_type_ascii)) {
    // Check if the codecs are a perfect match.
    std::vector<std::string> strict_codecs;
    media::ParseCodecString(ToASCIIOrEmpty(codecs), &strict_codecs, false);
    return static_cast<WebMimeRegistry::SupportsType>(
        media::IsSupportedStrictMediaMimeType(mime_type_ascii, strict_codecs));
  }

  // If we don't recognize the codec, it's possible we support it.
  std::vector<std::string> parsed_codecs;
  media::ParseCodecString(ToASCIIOrEmpty(codecs), &parsed_codecs, true);
  if (!media::AreSupportedMediaCodecs(parsed_codecs))
    return MayBeSupported;

  // Otherwise we have a perfect match.
  return IsSupported;
}

bool WebMimeRegistryImpl::supportsMediaSourceMIMEType(
    const blink::WebString& mime_type,
    const blink::WebString& codecs) {
  const std::string mime_type_ascii = ToASCIIOrEmpty(mime_type);
  if (mime_type_ascii.empty())
    return false;

  std::vector<std::string> parsed_codec_ids;
  media::ParseCodecString(ToASCIIOrEmpty(codecs), &parsed_codec_ids, false);
  return media::StreamParserFactory::IsTypeSupported(mime_type_ascii,
                                                     parsed_codec_ids);
}

blink::WebMimeRegistry::SupportsType
    WebMimeRegistryImpl::supportsNonImageMIMEType(
    const blink::WebString& mime_type) {
  return mime_util::IsSupportedNonImageMimeType(ToASCIIOrEmpty(mime_type))
             ? blink::WebMimeRegistry::IsSupported
             : blink::WebMimeRegistry::IsNotSupported;
}

blink::WebString WebMimeRegistryImpl::mimeTypeForExtension(
    const blink::WebString& file_extension) {
  std::string mime_type;
  net::GetMimeTypeFromExtension(
      base::FilePath::FromUTF16Unsafe(file_extension).value(), &mime_type);
  return blink::WebString::fromUTF8(mime_type);
}

blink::WebString WebMimeRegistryImpl::wellKnownMimeTypeForExtension(
    const blink::WebString& file_extension) {
  std::string mime_type;
  net::GetWellKnownMimeTypeFromExtension(
      base::FilePath::FromUTF16Unsafe(file_extension).value(), &mime_type);
  return blink::WebString::fromUTF8(mime_type);
}

blink::WebString WebMimeRegistryImpl::mimeTypeFromFile(
    const blink::WebString& file_path) {
  std::string mime_type;
  net::GetMimeTypeFromFile(base::FilePath::FromUTF16Unsafe(file_path),
                           &mime_type);
  return blink::WebString::fromUTF8(mime_type);
}

}  // namespace html_viewer
