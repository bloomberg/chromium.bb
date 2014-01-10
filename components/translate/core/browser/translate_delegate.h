// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DELEGATE_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DELEGATE_H_

namespace net {
class URLRequestContextGetter;
}

// A concrete instance of TranslateDelegate has to be provided by the embedder
// of the Translate component.
class TranslateDelegate {
 public:
  //Returns the URL request context in which the Translate component should make
  //requests.
  virtual net::URLRequestContextGetter* GetURLRequestContext() = 0;
};

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DELEGATE_H_
