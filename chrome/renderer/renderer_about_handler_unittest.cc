// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/about_handler.h"
#include "chrome/renderer/about_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

// This is just to make sure the about_urls array in
// chrome/common/about_handler.cc matches up with the about_urls_handlers
// in chrome/renderer/about_handler.cc. They used to be in one array, but
// we broke them apart to break a browser <-> renderer dependency.
// We cannot test this with COMPILE_ASSERT because
// chrome/renderer/about_handler.cc doesn't know about the size of about_urls
// in chrome/common/about_handler.cc at compile time.

TEST(RendererAboutHandlerTest, AboutUrlHandlerArray) {
  ASSERT_EQ(chrome_about_handler::about_urls_size,
            AboutHandler::AboutURLHandlerSize());
}
