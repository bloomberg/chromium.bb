// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/sample_activity_factory.h"

#include <string>

#include "athena/test/sample_activity.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace athena {
namespace test {

namespace {
const SkColor kDefaultColor = SK_ColorRED;
const SkColor kDefaultContentColor = SK_ColorGREEN;

const SkColor kDefaultAppColor = SK_ColorYELLOW;
const SkColor kDefaultAppContentColor = SK_ColorBLUE;
}

SampleActivityFactory::SampleActivityFactory() {}

SampleActivityFactory::~SampleActivityFactory() {}

Activity* SampleActivityFactory::CreateWebActivity(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return new SampleActivity(
      kDefaultColor, kDefaultContentColor, base::UTF8ToUTF16(url.spec()));
}

Activity* SampleActivityFactory::CreateAppActivity(
    apps::ShellAppWindow* app_window) {
  // SampleActivityFactory can't own the |app_window|, so it must be NULL.
  DCHECK(app_window == NULL);
  return new SampleActivity(
      kDefaultAppColor, kDefaultAppContentColor, base::UTF8ToUTF16("App"));
}

}  // namespace test
}  // namespace athena
