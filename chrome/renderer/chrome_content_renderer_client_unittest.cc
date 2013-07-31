// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "content/public/common/webplugininfo.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "url/gurl.h"

using WebKit::WebPluginParams;
using WebKit::WebString;
using WebKit::WebVector;
using chrome::ChromeContentRendererClient;
using content::WebPluginInfo;
using content::WebPluginMimeType;

namespace chrome {

namespace {
const bool kNaClRestricted = false;
const bool kNaClUnrestricted = true;
const bool kExtensionRestricted = false;
const bool kExtensionUnrestricted = true;
const bool kExtensionNotFromWebStore = false;
const bool kExtensionFromWebStore = true;
const bool kNotHostedApp = false;
const bool kHostedApp = true;

const char kNaClMimeType[] = "application/x-nacl";
const char kExtensionUrl[] = "chrome-extension://extension_id/background.html";

const char kAllowedNaClAppURL1[] = "https://plus.google.com";
const char kAllowedNaClAppURL2[] = "https://plus.sandbox.google.com";
const char kAllowedNaClManifestURL1[] = "https://ssl.gstatic.com/s2/oz/nacl/foo";
const char kAllowedNaClManifestURL2[] = "https://ssl.gstatic.com/photos/nacl/foo";

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
  mime_type_info.additional_param_names.push_back(UTF8ToUTF16("nacl"));
  mime_type_info.additional_param_values.push_back(
      UTF8ToUTF16(manifest_url));
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
    url_list->Append(base::Value::CreateStringValue(app_url));
    manifest.Set(extension_manifest_keys::kWebURLs, url_list);
    manifest.SetString(extension_manifest_keys::kLaunchWebURL, app_url);
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
    // Whitelisted manifest URL #1, whitelisted app URL root #1 is allowed.
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kAllowedNaClManifestURL1),
        GURL(kAllowedNaClAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    // Whitelisted manifest URL #2, whitelisted app URL root #1 is allowed.
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kAllowedNaClManifestURL1),
        GURL(kAllowedNaClAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));
    // Whitelisted manifest URL #1, whitelisted app URL root #2 is allowed.
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kAllowedNaClManifestURL1),
        GURL(kAllowedNaClAppURL2),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(AllowsDevInterfaces(params));

    // Whitelisted manifest URL, bad app URLs, NOT allowed.
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kAllowedNaClManifestURL1),
        GURL("http://plus.google.com/foo"),  // http scheme
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kAllowedNaClManifestURL1),
        GURL("http://plus.sandbox.google.com/foo"),  // http scheme
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kAllowedNaClManifestURL1),
        GURL("https://plus.google.evil.com/foo"),  // bad host
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    // Whitelisted app URL, bad manifest URL, NOT allowed.
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL("http://ssl.gstatic.com/s2/oz/nacl/foo"),  // http scheme
        GURL(kAllowedNaClAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL("https://ssl.gstatic.evil.com/s2/oz/nacl/foo"),  // bad host
        GURL(kAllowedNaClAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL("https://ssl.gstatic.com/wrong/s2/oz/nacl/foo"),  // bad path
        GURL(kAllowedNaClAppURL1),
        kNaClRestricted,
        CreateExtension(kExtensionRestricted, kExtensionNotFromWebStore).get(),
        &params));
  }
  // Whitelisted URLs can't get 'dev' interfaces with --enable-nacl.
  {
    WebPluginParams params;
    EXPECT_TRUE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(kAllowedNaClManifestURL1),
        GURL(kAllowedNaClAppURL1),
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
        GURL(kAllowedNaClManifestURL1),
        GURL(kAllowedNaClAppURL1),
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
        GURL("https://plus.google.com.evil.com/foo3"),
        kNaClRestricted,
        CreateExtension(kExtensionUnrestricted, kExtensionNotFromWebStore)
            .get(),
        &params));
    EXPECT_FALSE(ChromeContentRendererClient::IsNaClAllowed(
        GURL(),
        GURL("https://plus.google.com.evil.com/foo4"),
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

}  // namespace chrome
