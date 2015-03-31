// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBMIMEREGISTRY_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBMIMEREGISTRY_IMPL_H_

#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebMimeRegistry.h"

namespace html_viewer {

class WebMimeRegistryImpl : public blink::WebMimeRegistry {
 public:
  WebMimeRegistryImpl() {}
  virtual ~WebMimeRegistryImpl() {}

  // WebMimeRegistry methods:
  blink::WebMimeRegistry::SupportsType supportsMIMEType(
      const blink::WebString& mime_type) override;
  blink::WebMimeRegistry::SupportsType supportsImageMIMEType(
      const blink::WebString& mime_type) override;
  blink::WebMimeRegistry::SupportsType supportsImagePrefixedMIMEType(
      const blink::WebString& mime_type) override;
  blink::WebMimeRegistry::SupportsType supportsJavaScriptMIMEType(
      const blink::WebString& mime_type) override;
  blink::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const blink::WebString& mime_type,
      const blink::WebString& codecs,
      const blink::WebString& key_system) override;
  bool supportsMediaSourceMIMEType(const blink::WebString& mime_type,
                                   const blink::WebString& codecs) override;
  blink::WebMimeRegistry::SupportsType supportsNonImageMIMEType(
      const blink::WebString& mime_type) override;
  blink::WebString mimeTypeForExtension(
      const blink::WebString& extension) override;
  blink::WebString wellKnownMimeTypeForExtension(
      const blink::WebString& extension) override;
  blink::WebString mimeTypeFromFile(const blink::WebString& path) override;
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBMIMEREGISTRY_IMPL_H_
