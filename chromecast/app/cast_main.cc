// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/app/cast_main_delegate.h"
#include "content/public/app/content_main.h"

int main(int argc, const char** argv) {
  chromecast::shell::CastMainDelegate delegate(argc, argv);
  content::ContentMainParams params(&delegate);
  params.argc = delegate.argc();
  params.argv = delegate.argv();
  return content::ContentMain(params);
}
