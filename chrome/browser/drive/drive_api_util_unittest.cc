// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_api_util.h"

#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace drive {
namespace util {

TEST(DriveApiUtilTest, EscapeQueryStringValue) {
  EXPECT_EQ("abcde", EscapeQueryStringValue("abcde"));
  EXPECT_EQ("\\'", EscapeQueryStringValue("'"));
  EXPECT_EQ("\\'abcde\\'", EscapeQueryStringValue("'abcde'"));
  EXPECT_EQ("\\\\", EscapeQueryStringValue("\\"));
  EXPECT_EQ("\\\\\\'", EscapeQueryStringValue("\\'"));
}

TEST(DriveApiUtilTest, TranslateQuery) {
  EXPECT_EQ("", TranslateQuery(""));
  EXPECT_EQ("fullText contains 'dog'", TranslateQuery("dog"));
  EXPECT_EQ("fullText contains 'dog' and fullText contains 'cat'",
            TranslateQuery("dog cat"));
  EXPECT_EQ("not fullText contains 'cat'", TranslateQuery("-cat"));
  EXPECT_EQ("fullText contains 'dog cat'", TranslateQuery("\"dog cat\""));

  // Should handles full-width white space correctly.
  // Note: \xE3\x80\x80 (\u3000) is Ideographic Space (a.k.a. Japanese
  //   full-width whitespace).
  EXPECT_EQ("fullText contains 'dog' and fullText contains 'cat'",
            TranslateQuery("dog" "\xE3\x80\x80" "cat"));

  // If the quoted token is not closed (i.e. the last '"' is missing),
  // we handle the remaining string is one token, as a fallback.
  EXPECT_EQ("fullText contains 'dog cat'", TranslateQuery("\"dog cat"));

  // For quoted text with leading '-'.
  EXPECT_EQ("not fullText contains 'dog cat'", TranslateQuery("-\"dog cat\""));

  // Empty tokens should be simply ignored.
  EXPECT_EQ("", TranslateQuery("-"));
  EXPECT_EQ("", TranslateQuery("\"\""));
  EXPECT_EQ("", TranslateQuery("-\"\""));
  EXPECT_EQ("", TranslateQuery("\"\"\"\""));
  EXPECT_EQ("", TranslateQuery("\"\" \"\""));
  EXPECT_EQ("fullText contains 'dog'", TranslateQuery("\"\" dog \"\""));
}

TEST(FileSystemUtilTest, ExtractResourceIdFromUrl) {
  EXPECT_EQ("file:2_file_resource_id", ExtractResourceIdFromUrl(
      GURL("https://file1_link_self/file:2_file_resource_id")));
  // %3A should be unescaped.
  EXPECT_EQ("file:2_file_resource_id", ExtractResourceIdFromUrl(
      GURL("https://file1_link_self/file%3A2_file_resource_id")));

  // The resource ID cannot be extracted, hence empty.
  EXPECT_EQ("", ExtractResourceIdFromUrl(GURL("https://www.example.com/")));
}

TEST(FileSystemUtilTest, CanonicalizeResourceId) {
  std::string resource_id("1YsCnrMxxgp7LDdtlFDt-WdtEIth89vA9inrILtvK-Ug");

  // New style ID is unchanged.
  EXPECT_EQ(resource_id, CanonicalizeResourceId(resource_id));

  // Drop prefixes from old style IDs.
  EXPECT_EQ(resource_id, CanonicalizeResourceId("document:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("spreadsheet:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("presentation:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("drawing:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("table:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("externalapp:" + resource_id));
}

TEST(FileSystemUtilTest, ConvertAccountMetadataToAboutResource) {
  google_apis::AccountMetadata account_metadata;
  // Set up AccountMetadata instance.
  {
    account_metadata.set_quota_bytes_total(10000);
    account_metadata.set_quota_bytes_used(1000);
    account_metadata.set_largest_changestamp(100);
  }

  scoped_ptr<google_apis::AboutResource> about_resource(
      ConvertAccountMetadataToAboutResource(
          account_metadata, "dummy_root_id"));

  EXPECT_EQ(10000, about_resource->quota_bytes_total());
  EXPECT_EQ(1000, about_resource->quota_bytes_used());
  EXPECT_EQ(100, about_resource->largest_change_id());
  EXPECT_EQ("dummy_root_id", about_resource->root_folder_id());
}

TEST(FileSystemUtilTest, ConvertAccountMetadataToAppList) {
  google_apis::AccountMetadata account_metadata;
  // Set up AccountMetadata instance.
  {
    ScopedVector<google_apis::InstalledApp> installed_apps;
    scoped_ptr<google_apis::InstalledApp> installed_app(
        new google_apis::InstalledApp);
    installed_app->set_app_id("app_id");
    installed_app->set_app_name("name");
    installed_app->set_object_type("object_type");
    installed_app->set_supports_create(true);

    {
      ScopedVector<google_apis::Link> links;
      scoped_ptr<google_apis::Link> link(new google_apis::Link);
      link->set_type(google_apis::Link::LINK_PRODUCT);
      link->set_href(GURL("http://product/url"));
      links.push_back(link.release());
      installed_app->set_links(links.Pass());
    }
    {
      ScopedVector<std::string> primary_mimetypes;
      primary_mimetypes.push_back(new std::string("primary_mimetype"));
      installed_app->set_primary_mimetypes(primary_mimetypes.Pass());
    }
    {
      ScopedVector<std::string> secondary_mimetypes;
      secondary_mimetypes.push_back(new std::string("secondary_mimetype"));
      installed_app->set_secondary_mimetypes(secondary_mimetypes.Pass());
    }
    {
      ScopedVector<std::string> primary_extensions;
      primary_extensions.push_back(new std::string("primary_extension"));
      installed_app->set_primary_extensions(primary_extensions.Pass());
    }
    {
      ScopedVector<std::string> secondary_extensions;
      secondary_extensions.push_back(new std::string("secondary_extension"));
      installed_app->set_secondary_extensions(secondary_extensions.Pass());
    }
    {
      ScopedVector<google_apis::AppIcon> app_icons;
      scoped_ptr<google_apis::AppIcon> app_icon(new google_apis::AppIcon);
      app_icon->set_category(google_apis::AppIcon::ICON_DOCUMENT);
      app_icon->set_icon_side_length(10);
      {
        ScopedVector<google_apis::Link> links;
        scoped_ptr<google_apis::Link> link(new google_apis::Link);
        link->set_type(google_apis::Link::LINK_ICON);
        link->set_href(GURL("http://icon/url"));
        links.push_back(link.release());
        app_icon->set_links(links.Pass());
      }
      app_icons.push_back(app_icon.release());
      installed_app->set_app_icons(app_icons.Pass());
    }

    installed_apps.push_back(installed_app.release());
    account_metadata.set_installed_apps(installed_apps.Pass());
  }

  scoped_ptr<google_apis::AppList> app_list(
      ConvertAccountMetadataToAppList(account_metadata));
  const ScopedVector<google_apis::AppResource>& items = app_list->items();
  ASSERT_EQ(1U, items.size());

  const google_apis::AppResource& app_resource = *items[0];
  EXPECT_EQ("app_id", app_resource.application_id());
  EXPECT_EQ("name", app_resource.name());
  EXPECT_EQ("object_type", app_resource.object_type());
  EXPECT_TRUE(app_resource.supports_create());
  EXPECT_EQ("http://product/url", app_resource.product_url().spec());
  const ScopedVector<std::string>& primary_mimetypes =
      app_resource.primary_mimetypes();
  ASSERT_EQ(1U, primary_mimetypes.size());
  EXPECT_EQ("primary_mimetype", *primary_mimetypes[0]);
  const ScopedVector<std::string>& secondary_mimetypes =
      app_resource.secondary_mimetypes();
  ASSERT_EQ(1U, secondary_mimetypes.size());
  EXPECT_EQ("secondary_mimetype", *secondary_mimetypes[0]);
  const ScopedVector<std::string>& primary_file_extensions =
      app_resource.primary_file_extensions();
  ASSERT_EQ(1U, primary_file_extensions.size());
  EXPECT_EQ("primary_extension", *primary_file_extensions[0]);
  const ScopedVector<std::string>& secondary_file_extensions =
      app_resource.secondary_file_extensions();
  ASSERT_EQ(1U, secondary_file_extensions.size());
  EXPECT_EQ("secondary_extension", *secondary_file_extensions[0]);

  const ScopedVector<google_apis::DriveAppIcon>& icons = app_resource.icons();
  ASSERT_EQ(1U, icons.size());
  const google_apis::DriveAppIcon& icon = *icons[0];
  EXPECT_EQ(google_apis::DriveAppIcon::DOCUMENT, icon.category());
  EXPECT_EQ(10, icon.icon_side_length());
  EXPECT_EQ("http://icon/url", icon.icon_url().spec());
}

}  // namespace util
}  // namespace drive
