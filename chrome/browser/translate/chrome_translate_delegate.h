// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_DELEGATE_H_
#define CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_DELEGATE_H_

#include "components/translate/core/browser/translate_delegate.h"

#include <string>

#include "base/compiler_specific.h"

template <typename T>
struct DefaultSingletonTraits;

namespace net {
class URLRequestContextGetter;
}

// Chrome embedder for the translate component.
class ChromeTranslateDelegate : public TranslateDelegate {
 public:
  // Returns the singleton instance.
  // TODO(droger): Improve the ownership model. See http://crbug.com/332736.
  static ChromeTranslateDelegate* GetInstance();

  virtual ~ChromeTranslateDelegate();

 private:
  friend struct DefaultSingletonTraits<ChromeTranslateDelegate>;
  ChromeTranslateDelegate();

  // TranslateDelegate methods.
  virtual net::URLRequestContextGetter* GetURLRequestContext() OVERRIDE;
};

#endif  // CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_DELEGATE_H_
