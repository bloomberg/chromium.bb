// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_
#define HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_

#include "headless/public/headless_export.h"

namespace headless {
class HeadlessBrowserImpl;

// Represents an isolated session with a unique cache, cookies, and other
// profile/session related data.
class HEADLESS_EXPORT HeadlessBrowserContext {
 public:
  class Builder;

  virtual ~HeadlessBrowserContext() {}

  // TODO(skyostil): Allow saving and restoring contexts (crbug.com/617931).

 protected:
  HeadlessBrowserContext() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserContext);
};

class HEADLESS_EXPORT HeadlessBrowserContext::Builder {
 public:
  Builder(Builder&&);
  ~Builder();

  // TODO(skyostil): Allow overriding protocol handlers.

  std::unique_ptr<HeadlessBrowserContext> Build();

 private:
  friend class HeadlessBrowserImpl;

  explicit Builder(HeadlessBrowserImpl* browser);

  HeadlessBrowserImpl* browser_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_
