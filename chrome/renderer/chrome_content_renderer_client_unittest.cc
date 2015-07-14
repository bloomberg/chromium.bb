// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/renderer/searchbox/search_bouncer.h"
#include "content/public/common/webplugininfo.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#endif

#if !defined(DISABLE_NACL)
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#endif

#if !defined(DISABLE_NACL)
using blink::WebPluginParams;
using blink::WebString;
using blink::WebVector;
#endif

using content::WebPluginInfo;
using content::WebPluginMimeType;

namespace {

#if !defined(DISABLE_NACL)
const bool kNaClRestricted = false;
const bool kNaClUnrestricted = true;
const bool kExtensionNotFromWebStore = false;
const bool kExtensionFromWebStore = true;
#endif

#if defined(ENABLE_EXTENSIONS)
const bool kNotHostedApp = false;
const bool kHostedApp = true;
#endif

#if !defined(DISABLE_NACL)
const char kExtensionUrl[] = "chrome-extension://extension_id/background.html";

const char kPhotosAppURL1[] = "https://foo.plus.google.com";
const char kPhotosAppURL2[] = "https://foo.plus.sandbox.google.com";
const char kPhotosManifestURL1[] = "https://ssl.gstatic.com/s2/oz/nacl/foo";
const char kPhotosManifestURL2[] = "https://ssl.gstatic.com/photos/nacl/foo";
const char kChatManifestFS1[] =
  "filesystem:https://foo.talkgadget.google.com/foo";
const char kChatManifestFS2[] = "filesystem:https://foo.plus.google.com/foo";
const char kChatManifestFS3[] =
  "filesystem:https://foo.plus.sandbox.google.com/foo";
#endif

const char kChatAppURL1[] = "https://foo.talkgadget.google.com/hangouts/foo";
const char kChatAppURL2[] = "https://foo.plus.google.com/hangouts/foo";
const char kChatAppURL3[] = "https://foo.plus.sandbox.google.com/hangouts/foo";

#if !defined(DISABLE_NACL)
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
#endif

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


#if defined(ENABLE_EXTENSIONS)
scoped_refptr<const extensions::Extension> CreateTestExtension(
    extensions::Manifest::Location location, bool is_from_webstore,
    bool is_hosted_app, const std::string& app_url) {
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
    bool is_from_webstore) {
  return CreateTestExtension(
      extensions::Manifest::INTERNAL, is_from_webstore, kNotHostedApp,
      std::string());
}

scoped_refptr<const extensions::Extension> CreateExtensionWithLocation(
    extensions::Manifest::Location location, bool is_from_webstore) {
  return CreateTestExtension(
      location, is_from_webstore, kNotHostedApp, std::string());
}

scoped_refptr<const extensions::Extension> CreateHostedApp(
    bool is_from_webstore, const std::string& app_url) {
  return CreateTestExtension(extensions::Manifest::INTERNAL,
                             is_from_webstore,
                             kHostedApp,
                             app_url);
}
#endif  // defined(ENABLE_EXTENSIONS)

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
#if !defined(DISABLE_NACL)
  // --enable-nacl allows all NaCl apps, with 'dev' interfaces.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL(),
        kNaClUnrestricted,
        CreateExtension(kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_TRUE(AllowsDevInterfaces(params));
  }
  // Unpacked extensions are allowed without --enable-nacl, with
  // 'dev' interfaces.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL(kExtensionUrl),
        kNaClRestricted,
        CreateExtensionWithLocation(extensions::Manifest::UNPACKED,
                                    kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_TRUE(AllowsDevInterfaces(params));
  }
  // Component extensions are allowed without --enable-nacl, with
  // 'dev' interfaces.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL(kExtensionUrl),
        kNaClRestricted,
        CreateExtensionWithLocation(extensions::Manifest::COMPONENT,
                                    kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_TRUE(AllowsDevInterfaces(params));
  }
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL(kExtensionUrl),
        kNaClRestricted,
        CreateExtensionWithLocation(extensions::Manifest::EXTERNAL_COMPONENT,
                                    kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_TRUE(AllowsDevInterfaces(params));
  }
  // Extensions that are force installed by policy are allowed without
  // --enable-nacl, without 'dev' interfaces.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL(kExtensionUrl),
        kNaClRestricted,
        CreateExtensionWithLocation(extensions::Manifest::EXTERNAL_POLICY,
                                    kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL(kExtensionUrl),
        kNaClRestricted,
        CreateExtensionWithLocation(
            extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD,
            kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
  }
  // CWS extensions are allowed without --enable-nacl, without 'dev'
  // interfaces if called from an extension url.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL(kExtensionUrl),
        kNaClRestricted,
        CreateExtension(kExtensionFromWebStore).get(),
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
        CreateExtension(kExtensionFromWebStore).get(),
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
        CreateExtension(kExtensionFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
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
        nullptr,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL(kPhotosAppURL2),
        kNaClRestricted,
        nullptr,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL2),
        GURL(kPhotosAppURL1),
        kNaClRestricted,
        nullptr,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL2),
        GURL(kPhotosAppURL2),
        kNaClRestricted,
        nullptr,
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    // Whitelisted Chat app is allowed.
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kChatManifestFS1),
        GURL(kChatAppURL1),
        kNaClRestricted,
        nullptr,
        &params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kChatManifestFS2),
        GURL(kChatAppURL2),
        kNaClRestricted,
        nullptr,
        &params));
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kChatManifestFS3),
        GURL(kChatAppURL3),
        kNaClRestricted,
        nullptr,
        &params));

    // Whitelisted manifest URL, bad app URLs, NOT allowed.
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL("http://plus.google.com/foo"),  // http scheme
        kNaClRestricted,
        nullptr,
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL("http://plus.sandbox.google.com/foo"),  // http scheme
        kNaClRestricted,
        nullptr,
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL("https://plus.google.evil.com/foo"),  // bad host
        kNaClRestricted,
        nullptr,
        &params));
    // Whitelisted app URL, bad manifest URL, NOT allowed.
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL("http://ssl.gstatic.com/s2/oz/nacl/foo"),  // http scheme
        GURL(kPhotosAppURL1),
        kNaClRestricted,
        nullptr,
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL("https://ssl.gstatic.evil.com/s2/oz/nacl/foo"),  // bad host
        GURL(kPhotosAppURL1),
        kNaClRestricted,
        nullptr,
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL("https://ssl.gstatic.com/wrong/s2/oz/nacl/foo"),  // bad path
        GURL(kPhotosAppURL1),
        kNaClRestricted,
        nullptr,
        &params));
  }
  // Whitelisted URLs can't get 'dev' interfaces with --enable-nacl.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kPhotosManifestURL1),
        GURL(kPhotosAppURL1),
        kNaClUnrestricted,
        nullptr,
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
        nullptr,
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
        nullptr,
        &params));
  }
  // Non chrome-extension:// URLs belonging to hosted apps are allowed for
  // webstore installed hosted apps.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL("http://example.com/test.html"),
        kNaClRestricted,
        CreateHostedApp(kExtensionFromWebStore,
                        "http://example.com/").get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL("http://example.com/test.html"),
        kNaClRestricted,
        CreateHostedApp(kExtensionNotFromWebStore,
                        "http://example.com/").get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL("http://example.evil.com/test.html"),
        kNaClRestricted,
        CreateHostedApp(kExtensionNotFromWebStore,
                        "http://example.com/").get(),
        &params));
  }
#endif  // !defined(DISABLE_NACL)
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
  SearchBouncer::GetInstance()->OnSetSearchURLs(
      std::vector<GURL>(), GURL("http://example.com/n"));
  EXPECT_FALSE(client.ShouldSuppressErrorPage(nullptr,
                                              GURL("http://example.com")));
  EXPECT_TRUE(client.ShouldSuppressErrorPage(nullptr,
                                             GURL("http://example.com/n")));
  SearchBouncer::GetInstance()->OnSetSearchURLs(
      std::vector<GURL>(), GURL::EmptyGURL());
}
