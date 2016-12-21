// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.

#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/common_param_traits_macros.h"
#include "chrome/common/mac/app_shim_messages.h"
#include "chrome/common/page_load_metrics/page_load_metrics_messages.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/tts_messages.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/common_param_traits_macros.h"
#include "extensions/features/features.h"
#include "media/media_features.h"
#include "printing/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/cast_messages.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/common/service_messages.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/common/chrome_utility_printing_messages.h"
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
#include "chrome/common/media/webrtc_logging_messages.h"
#endif
