// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "webkit/plugins/webplugininfo.h"

using WebKit::WebPluginParams;
using WebKit::WebString;
using WebKit::WebVector;
using chrome::ChromeContentRendererClient;
using webkit::WebPluginInfo;
using webkit::WebPluginMimeType;

namespace chrome {

namespace {
const bool kNaClRestricted = false;
const bool kNaClUnrestricted = true;
const bool kExtensionRestricted = false;
const bool kExtensionUnrestricted = true;
const bool kExtensionNotFromWebStore = false;
const bool kExtensionFromWebStore = true;

const char kNaClMimeType[] = "application/x-nacl";

bool AllowsDevInterfaces(const WebPluginParams& params) {
  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    if (params.attributeNames[i] == WebString::fromUTF8("@dev"))
      return true;
  }
  return false;
}

void AddFakeDevAttribute(WebPluginParams* params) {
  WebVector<WebString> names(static_cast<size_t>(1));
  WebVector<WebString> values(static_cast<size_t>(1));
  names[0] = WebString::fromUTF8("@dev");
  values[0] = WebString();
  params->attributeNames.swap(names);
  params->attributeValues.swap(values);
}

void AddContentTypeHandler(WebPluginInfo* info,
                           const char* mime_type,
                           const char* manifest_url) {
  WebPluginMimeType mime_type_info;
  mime_type_info.mime_type = mime_type;
  mime_type_info.additional_param_names.push_back(UTF8ToUTF16("nacl"));
  mime_type_info.additional_param_values.push_back(
      UTF8ToUTF16(manifest_url));
  info->mime_types.push_back(mime_type_info);
}
}  // namespace

typedef testing::Test ChromeContentRendererClientTest;

TEST_F(ChromeContentRendererClientTest, NaClRestriction) {
  // Unknown content types have no NaCl module.
  {
    WebPluginInfo info;
    EXPECT_EQ(GURL(),
              ChromeContentRendererClient::GetNaClContentHandlerURL(
                  "application/x-foo", info));
  }
  // Known content types have a NaCl module.
  {
    WebPluginInfo info;
    AddContentTypeHandler(&info, "application/x-foo", "www.foo.com");
    EXPECT_EQ(GURL("www.foo.com"),
              ChromeContentRendererClient::GetNaClContentHandlerURL(
                  "application/x-foo", info));
  }
  // --enable-nacl allows all NaCl apps, with 'dev' interfaces.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL(), kNaClUnrestricted, kExtensionRestricted,
        kExtensionNotFromWebStore, &params));
    EXPECT_TRUE(AllowsDevInterfaces(params));
  }
  // Unrestricted extensions are allowed without --enable-nacl, with 'dev'
  // interfaces.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL(), kNaClRestricted, kExtensionUnrestricted,
        kExtensionNotFromWebStore, &params));
    EXPECT_TRUE(AllowsDevInterfaces(params));
  }
  // CWS extensions are allowed without --enable-nacl, without 'dev'
  // interfaces.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL(), kNaClRestricted, kExtensionRestricted,
        kExtensionFromWebStore, &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // CWS extensions can't get 'dev' interfaces with --enable-nacl.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL(), kNaClUnrestricted, kExtensionRestricted,
        kExtensionFromWebStore, &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // CWS extensions can't get 'dev' interfaces by injecting a fake
  // '@dev' attribute.
  {
    WebPluginParams params;
    AddFakeDevAttribute(&params);
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL(), kNaClRestricted, kExtensionRestricted,
        kExtensionFromWebStore, &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // The NaCl PDF extension is allowed without --enable-nacl, with 'dev'
  // interfaces.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL("chrome-extension://acadkphlmlegjaadjagenfimbpphcgnh"),
        GURL(), kNaClRestricted, kExtensionRestricted,
        kExtensionFromWebStore, &params));
    EXPECT_TRUE(AllowsDevInterfaces(params));
  }
  // Whitelisted URLs are allowed without --enable-nacl, without 'dev'
  // interfaces.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL("http://plus.google.com/games"),
        kNaClRestricted, kExtensionRestricted, kExtensionNotFromWebStore,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL("https://plus.google.com/games"),
        kNaClRestricted, kExtensionRestricted, kExtensionNotFromWebStore,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL("https://plus.google.com/games/209089085730"),
        kNaClRestricted, kExtensionRestricted, kExtensionNotFromWebStore,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL("http://plus.sandbox.google.com/games"),
        kNaClRestricted, kExtensionRestricted, kExtensionNotFromWebStore,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL("https://plus.sandbox.google.com/games"),
        kNaClRestricted, kExtensionRestricted, kExtensionNotFromWebStore,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL("https://plus.google.com/games/209089085730"),
        kNaClRestricted, kExtensionRestricted, kExtensionNotFromWebStore,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // Whitelisted URLs can't get 'dev' interfaces with --enable-nacl.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL("https://plus.google.com/games/209089085730"),
        kNaClUnrestricted, kExtensionRestricted, kExtensionNotFromWebStore,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // Whitelisted URLs can't get 'dev' interfaces by injecting a fake
  // '@dev' attribute.
  {
    WebPluginParams params;
    AddFakeDevAttribute(&params);
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL("https://plus.google.com/games/209089085730"),
        kNaClRestricted, kExtensionRestricted, kExtensionNotFromWebStore,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // Non-whitelisted URLs are blocked without --enable-nacl.
  {
    WebPluginParams params;
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(), GURL("http://plus.google.com.evil.com/games"),
        kNaClRestricted, kExtensionRestricted, kExtensionNotFromWebStore,
        &params));
  }
}

}  // namespace chrome

