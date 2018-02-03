// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/presentation/presentation_struct_traits.h"

#include "url/mojo/url_gurl_struct_traits.h"

namespace mojo {

bool StructTraits<blink::mojom::PresentationInfoDataView,
                  content::PresentationInfo>::
    Read(blink::mojom::PresentationInfoDataView data,
         content::PresentationInfo* out) {
  if (!data.ReadUrl(&(out->presentation_url)) ||
      !data.ReadId(&(out->presentation_id))) {
    return false;
  }

  if (out->presentation_id.empty() ||
      !base::IsStringASCII(out->presentation_id) ||
      out->presentation_id.length() > content::PresentationInfo::kMaxIdLength) {
    return false;
  }
  return true;
}

bool StructTraits<blink::mojom::PresentationErrorDataView,
                  content::PresentationError>::
    Read(blink::mojom::PresentationErrorDataView data,
         content::PresentationError* out) {
  if (!data.ReadErrorType(&(out->error_type)) ||
      !data.ReadMessage(&(out->message))) {
    return false;
  }

  if (!base::IsStringUTF8(out->message) ||
      out->message.length() > content::PresentationError::kMaxMessageLength) {
    return false;
  }

  return true;
}

bool UnionTraits<blink::mojom::PresentationConnectionMessageDataView,
                 content::PresentationConnectionMessage>::
    Read(blink::mojom::PresentationConnectionMessageDataView data,
         content::PresentationConnectionMessage* out) {
  if (data.is_message()) {
    if (!data.ReadMessage(&(out->message))) {
      return false;
    }
  } else {
    if (!data.ReadData(&(out->data))) {
      return false;
    }
  }
  return true;
}
}
