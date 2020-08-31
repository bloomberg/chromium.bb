// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_TEST_UTILS_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_TEST_UTILS_H_

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/browser/translate_waiter.h"
#include "components/translate/core/common/translate_errors.h"

namespace content {
class WebContents;
}

namespace translate {

// Creates a TranslateWaiter that listens for |wait_event| in the specified
// |web_contents|.
std::unique_ptr<TranslateWaiter> CreateTranslateWaiter(
    content::WebContents* web_contents,
    TranslateWaiter::WaitEvent wait_event);

}  // namespace translate

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_TEST_UTILS_H_
