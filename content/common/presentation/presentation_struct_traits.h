// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PRESENTATION_PRESENTATION_STRUCT_TRAITS_H_
#define CONTENT_COMMON_PRESENTATION_PRESENTATION_STRUCT_TRAITS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "content/public/common/presentation_connection_message.h"
#include "content/public/common/presentation_info.h"
#include "third_party/blink/public/platform/modules/presentation/presentation.mojom.h"
#include "url/mojom/url.mojom.h"

namespace mojo {

template <>
struct StructTraits<blink::mojom::PresentationInfoDataView,
                    content::PresentationInfo> {
  static const GURL& url(const content::PresentationInfo& presentation_info) {
    return presentation_info.presentation_url;
  }

  static const std::string& id(
      const content::PresentationInfo& presentation_info) {
    return presentation_info.presentation_id;
  }

  static bool Read(blink::mojom::PresentationInfoDataView data,
                   content::PresentationInfo* out);
};

template <>
struct UnionTraits<blink::mojom::PresentationConnectionMessageDataView,
                   content::PresentationConnectionMessage> {
  static blink::mojom::PresentationConnectionMessageDataView::Tag GetTag(
      const content::PresentationConnectionMessage& message) {
    return message.is_binary()
               ? blink::mojom::PresentationConnectionMessageDataView::Tag::DATA
               : blink::mojom::PresentationConnectionMessageDataView::Tag::
                     MESSAGE;
  }

  static const std::string& message(
      const content::PresentationConnectionMessage& message) {
    DCHECK(!message.is_binary());
    return message.message.value();
  }

  static const std::vector<uint8_t>& data(
      const content::PresentationConnectionMessage& message) {
    DCHECK(message.is_binary());
    return message.data.value();
  }

  static bool Read(blink::mojom::PresentationConnectionMessageDataView data,
                   content::PresentationConnectionMessage* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_PRESENTATION_PRESENTATION_STRUCT_TRAITS_H_
