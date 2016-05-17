// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MHTML_GENERATION_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_MHTML_GENERATION_PARAMS_H_

#include "base/files/file_path.h"
#include "content/common/content_export.h"

namespace content {

// Options for changing serialization behavior based on the CacheControl header
// of each subresource.
enum class MHTMLCacheControlPolicy {
  NONE = 0,
  FAIL_FOR_NO_STORE_MAIN_FRAME,

  // |LAST| is used in content/public/common/common_param_traits_macros.h with
  // IPC_ENUM_TRAITS_MAX_VALUE macro. Keep the value up to date. Otherwise
  // a new value can not be passed to the renderer.
  LAST = FAIL_FOR_NO_STORE_MAIN_FRAME
};

struct CONTENT_EXPORT MHTMLGenerationParams {
  MHTMLGenerationParams(const base::FilePath& file_path);
  ~MHTMLGenerationParams() = default;

  // The file that will contain the generated MHTML.
  base::FilePath file_path;

  // Uses Content-Transfer-Encoding: binary when encoding.  See
  // https://tools.ietf.org/html/rfc2045 for details about
  // Content-Transfer-Encoding.
  bool use_binary_encoding = false;

  // By default, MHTML includes all subresources.  This flag can be used to
  // cause the generator to fail or silently ignore resources if the
  // Cache-Control header is used.
  MHTMLCacheControlPolicy cache_control_policy = MHTMLCacheControlPolicy::NONE;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MHTML_GENERATION_PARAMS_H_
