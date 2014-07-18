// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/renderer/searchbox/search_bouncer.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "url/gurl.h"

using blink::WebPluginParams;
using blink::WebString;
using blink::WebVector;
using content::WebPluginInfo;
using content::WebPluginMimeType;

namespace {
const bool kNaClRestricted = false;
const bool kNaClUnrestricted = true;
const bool kExtensionRestricted = false;
const bool kExtensionUnrestricted = true;
const bool kExtensionNotFromWebStore = false;
const bool kExtensionFromWebStore = true;
const bool kNotHostedApp = false;
const bool kHostedApp = true;

const char kExtensionUrl[] = "chrome-extension://extension_id/background.html";

const char kPhotosAppURL1[] = "https://foo.plus.google.com";
const char kPhotosAppURL2[] = "https://foo.plus.sandbox.google.com";
const char kPhotosManifestURL1[] = "https://ssl.gstatic.com/s2/oz/nacl/foo";
const char kPhotosManifestURL2[] = "https://ssl.gstatic.com/photos/nacl/foo";

const char kChatAppURL1[] = "https://foo.talkgadget.google.com/hangouts/foo";
const char kChatAppURL2[] = "https://foo.plus.google.com/hangouts/foo";
const char kChatAppURL3[] = "https://foo.plus.sandbox.google.com/hangouts/foo";
const char kChatManifestFS1[] =
  "filesystem:https://foo.talkgadget.google.com/foo";
const char kChatManifestFS2[] = "filesystem:https://foo.plus.google.com/foo";
const char kChatManifestFS3[] =
  "filesystem:https://foo.plus.sandbox.google.com/foo";

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

void AddContentTypeHandler(content::WebPluginInfo* info,
                           const char* mime_type,
                           const char* manifest_url) {
  content::WebPluginMimeType mime_type_info;
  mime_type_info.mime_type = mime_type;
  mime_type_info.additional_param_names.push_back(base::UTF8ToUTF16("nacl"));
  mime_type_info.additional_param_values.push_back(
      base::UTF8ToUTF16(manifest_url));
  info->mime_types.push_back(mime_type_info);
}
}  // namespace

typedef testing::Test ChromeContentRendererClientTest;


scoped_refptr<const extensions::Extension> CreateTestExtension(
    bool is_unrestricted, bool is_from_webstore, bool is_hosted_app,
    const std::string& app_url) {
  extensions::Manifest::Location location = is_unrestricted ?
      extensions::Manifest::UNPACKED :
      extensions::Manifest::INTERNAL;
  int flags = is_from_webstore ?
      extensions::Extension::FROM_WEBSTORE:
      extensions::Extension::NO_FLAGS;

  base::DictionaryValue manifest;
  manifest.SetString("name", "NaCl Extension");
  manifest.SetString("version", "1");
  manifest.SetInteger("manifest_version", 2);
  if (is_hosted_app) {
    base::ListValue* url_list = new base::ListValue();
    url_list->Append(new base::StringValue(app_url));
    manifest.Set(extensions::manifest_keys::kWebURLs, url_list);
    manifest.SetString(extensions::manifest_keys::kLaunchWebURL, app_url);
  }
  std::string error;
  return extensions::Extension::Create(base::FilePath(), location, manifest,
                                       flags, &error);
}

scoped_refptr<const extensions::Extension> CreateExtension(
    bool is_unrestricted, bool is_from_webstore) {
  return CreateTestExtension(
      is_unrestricted, is_from_webstore, kNotHostedApp, std::string());
}

scoped_refptr<const extensions::Extension> CreateHostedApp(
    bool is_unrestricted, bool is_from_webstore, const std::string& app_url) {
  return CreateTestExtension(is_unrestricted, is_from_webstore, kHostedApp,
                             app_url);
}

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
        GURL(),
        GURL(),
        kNaClUnrestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_TRUE(AllowsDevInterfaces(params));
  }
  // Unrestricted extensions are allowed without --enable-nacl, with 'dev'
  // interfaces if called from an extension url.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL(kExtensionUrl),
        kNaClRestricted,
        CreateExtension(kExtensionUnrestricted, kExtensionNotFromWebStore)
            .get(),
        &params));
    EXPECT_TRUE(AllowsDevInterfaces(params));
  }
  // CWS extensions are allowed without --enable-nacl, without 'dev'
  // interfaces if called from an extension url.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL(kExtensionUrl),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // CWS extensions can't get 'dev' interfaces with --enable-nacl.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL(kExtensionUrl),
        kNaClUnrestricted,
        CreateExtension(kExtensionRestricted, kExtensionFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // CWS extensions can't get 'dev' interfaces by injecting a fake
  // '@dev' attribute.
  {
    WebPluginParams params;
    AddFakeDevAttribute(&params);
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL(kExtensionUrl),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // The NaCl PDF extension is allowed without --enable-nacl, with 'dev'
  // interfaces, from all URLs.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL("chrome-extension://acadkphlmlegjaadjagenfimbpphcgnh"),
        GURL(),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionFromWebStore).get(),
        &params));
    EXPECT_TRUE(AllowsDevInterfaces(params));
  }
  // Whitelisted URLs are allowed without --enable-nacl, without 'dev'
  // interfaces. There is a whitelist for the app URL and the manifest URL.
  {
    WebPluginParams params;
    // Whitelisted Photos app is allowed (two app URLs, two manifest URLs)
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL(kPhotosAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL(kPhotosAppURL2),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL2),
        GURL(kPhotosAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL2),
        GURL(kPhotosAppURL2),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    // Whitelisted Chat app is allowed.
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kChatManifestFS1),
        GURL(kChatAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kChatManifestFS2),
        GURL(kChatAppURL2),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kChatManifestFS3),
        GURL(kChatAppURL3),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));

    // Whitelisted manifest URL, bad app URLs, NOT allowed.
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL("http://plus.google.com/foo"),  // http scheme
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL("http://plus.sandbox.google.com/foo"),  // http scheme
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL("https://plus.google.evil.com/foo"),  // bad host
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    // Whitelisted app URL, bad manifest URL, NOT allowed.
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL("http://ssl.gstatic.com/s2/oz/nacl/foo"),  // http scheme
        GURL(kPhotosAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL("https://ssl.gstatic.evil.com/s2/oz/nacl/foo"),  // bad host
        GURL(kPhotosAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL("https://ssl.gstatic.com/wrong/s2/oz/nacl/foo"),  // bad path
        GURL(kPhotosAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
  }
  // Whitelisted URLs can't get 'dev' interfaces with --enable-nacl.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL(kPhotosAppURL1),
        kNaClUnrestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // Whitelisted URLs can't get 'dev' interfaces by injecting a fake
  // '@dev' attribute.
  {
    WebPluginParams params;
    AddFakeDevAttribute(&params);
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL(kPhotosAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // Non-whitelisted URLs are blocked without --enable-nacl.
  {
    WebPluginParams params;
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL("https://plus.google.com.evil.com/foo1"),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL("https://plus.google.com.evil.com/foo2"),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionFromWebStore).get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL("https://talkgadget.google.com.evil.com/foo3"),
        kNaClRestricted,
        CreateExtension(kExtensionUnrestricted, kExtensionNotFromWebStore)
            .get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL("https://talkgadget.google.com.evil.com/foo4"),
        kNaClRestricted,
        CreateExtension(kExtensionUnrestricted, kExtensionFromWebStore).get(),
        &params));
  }
  // Non chrome-extension:// URLs belonging to hosted apps are allowed.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL("http://example.com/test.html"),
        kNaClRestricted,
        CreateHostedApp(kExtensionRestricted,
                        kExtensionNotFromWebStore,
                        "http://example.com/").get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL("http://example.evil.com/test.html"),
        kNaClRestricted,
        CreateHostedApp(kExtensionRestricted,
                        kExtensionNotFromWebStore,
                        "http://example.com/").get(),
        &params));
  }
}

TEST_F(ChromeContentRendererClientTest, AllowPepperMediaStreamAPI) {
  ChromeContentRendererClient test;
#if !defined(OS_ANDROID)
  EXPECT_TRUE(test.AllowPepperMediaStreamAPI(GURL(kChatAppURL1)));
  EXPECT_TRUE(test.AllowPepperMediaStreamAPI(GURL(kChatAppURL2)));
  EXPECT_TRUE(test.AllowPepperMediaStreamAPI(GURL(kChatAppURL3)));
#else
  EXPECT_FALSE(test.AllowPepperMediaStreamAPI(GURL(kChatAppURL1)));
  EXPECT_FALSE(test.AllowPepperMediaStreamAPI(GURL(kChatAppURL2)));
  EXPECT_FALSE(test.AllowPepperMediaStreamAPI(GURL(kChatAppURL3)));
#endif
  EXPECT_FALSE(test.AllowPepperMediaStreamAPI(
      GURL("http://talkgadget.google.com/hangouts/foo")));
  EXPECT_FALSE(test.AllowPepperMediaStreamAPI(
      GURL("https://talkgadget.evil.com/hangouts/foo")));
}

TEST_F(ChromeContentRendererClientTest, ShouldSuppressErrorPage) {
  ChromeContentRendererClient client;
  client.search_bouncer_.reset(new SearchBouncer);
  client.search_bouncer_->OnSetSearchURLs(
      std::vector<GURL>(), GURL("http://example.com/n"));
  EXPECT_FALSE(client.ShouldSuppressErrorPage(NULL,
                                              GURL("http://example.com")));
  EXPECT_TRUE(client.ShouldSuppressErrorPage(NULL,
                                             GURL("http://example.com/n")));
}
