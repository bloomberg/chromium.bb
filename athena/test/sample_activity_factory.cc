// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/sample_activity_factory.h"

#include <string>

#include "athena/test/sample_activity.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace athena {
namespace test {

namespace {
const SkColor kDefaultColor = SK_ColorRED;
const SkColor kDefaultContentColor = SK_ColorGREEN;
}

SampleActivityFactory::SampleActivityFactory() {}

SampleActivityFactory::~SampleActivityFactory() {}

Activity* SampleActivityFactory::CreateWebActivity(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return new SampleActivity(
      kDefaultColor, kDefaultContentColor, url.spec());
}

}  // namespace test
}  // namespace athena
