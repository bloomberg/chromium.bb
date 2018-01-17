// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Test fixture to test plugin_placeholder.js.
typedef web::WebTestWithWebState PluginPlaceholderJsTest;

// Tests that __gCrWeb.plugin.updatePluginPlaceholders JavaScript API correctly
// find plugins which are candidates for covering with a placeholder.
// NOTE: Some of the plugins detected here may not actually be covered with a
// placeholder because __gCrWeb.plugin.addPluginPlaceholders also takes into
// account the physical size of the plugin.
TEST_F(PluginPlaceholderJsTest, UpdatePluginPlaceholders) {
  struct TestData {
    NSString* plugin_source;
    BOOL expected_placeholder_installed;
  } test_data[] = {
      // Applet with fallback data should be untouched.
      {@"<html><applet code='Some.class'><p>Fallback text.</p></applet>"
        "</body></html>",
       NO},
      // Applet without fallback data should be covered.
      {@"<html><applet code='Some.class'></applet></body></html>", YES},
      // Object with flash embed fallback should be covered.
      {@"<html><body>"
        "<object classid='clsid:D27CDB6E-AE6D-11cf-96B8-444553540000'"
        "    codebase='http://download.macromedia.com/pub/shockwave/cabs/'"
        "flash/swflash.cab#version=6,0,0,0'>"
        "  <param name='movie' value='some.swf'>"
        "  <embed src='some.swf' type='application/x-shockwave-flash'>"
        "</object>"
        "</body></html>",
       YES},
      // Object with undefined embed fallback should be untouched.
      {@"<html><body>"
        "<object classid='clsid:D27CDB6E-AE6D-11cf-96B8-444553540000'"
        "    codebase='http://download.macromedia.com/pub/shockwave/cabs/'"
        "flash/swflash.cab#version=6,0,0,0'>"
        "  <param name='movie' value='some.swf'>"
        "  <embed src='some.swf'>"
        "</object>"
        "</body></html>",
       NO},
      // Object with text fallback should be untouched.
      {@"<html><body>"
        "<object type='application/x-shockwave-flash' data='some.sfw'>"
        "  <param name='movie' value='some.swf'>"
        "  <p>Fallback text.</p>"
        "</object>"
        "</body></html>",
       NO},
      // Object with no fallback should be covered.
      {@"<html><body>"
        "<object type='application/x-shockwave-flash' data='some.sfw'>"
        "  <param name='movie' value='some.swf'>"
        "</object>"
        "</body></html>",
       YES},
      // Object displaying an image should be untouched.
      {@"<html><body>"
        "<object data='foo.png' type='image/png'>"
        "</object>"
        "</body></html>",
       NO},
  };
  for (size_t i = 0; i < arraysize(test_data); i++) {
    TestData& data = test_data[i];
    LoadHtml(data.plugin_source);
    id result =
        ExecuteJavaScript(@"__gCrWeb.plugin.updatePluginPlaceholders()");
    EXPECT_NSEQ(@(data.expected_placeholder_installed), result)
        << " in test " << i << ": "
        << base::SysNSStringToUTF8(data.plugin_source);
  }
}

}  // namespace web
