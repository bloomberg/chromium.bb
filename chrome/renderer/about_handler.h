// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code for handling "about:" URLs in the renderer process.  We handle
// most about: URLs in the browser process (see
// browser/browser_about_handler.*), but test URLs like about:crash need to
// happen in the renderer.

#ifndef CHROME_RENDERER_ABOUT_HANDLER_H__
#define CHROME_RENDERER_ABOUT_HANDLER_H__
#pragma once

#include "base/basictypes.h"

class GURL;

class AboutHandler {
 public:
  // Given a URL, determine whether or not to handle it specially.  Returns
  // true if the URL was handled.
  static bool MaybeHandle(const GURL& url);

  // Induces a renderer crash.
  static void AboutCrash();

  // Induces a renderer hang.
  static void AboutHang();

  // Induces a brief (20 second) hang to make sure hang monitors go away.
  static void AboutShortHang();

  // Returns the size of |about_urls_handlers|. Used for testing only.
  static size_t AboutURLHandlerSize();

 private:
  AboutHandler();
  ~AboutHandler();

  DISALLOW_COPY_AND_ASSIGN(AboutHandler);
};

#endif  // CHROME_RENDERER_ABOUT_HANDLER_H__
