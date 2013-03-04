// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_api_parser.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/google_apis/time_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;
using base::DictionaryValue;
using base::ListValue;

namespace google_apis {

// TODO(nhiroki): Make it possible to run these tests on any platforms after
// moving json files to out of 'chromeos' directory (http://crbug.com/149788).
#if defined(OS_CHROMEOS)
// Test about resource parsing.
TEST(DriveAPIParserTest, AboutResourceParser) {
  std::string error;
  scoped_ptr<Value> document = test_util::LoadJSONFile("drive/about.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<AboutResource> resource(new AboutResource());
  EXPECT_TRUE(resource->Parse(*document));

  EXPECT_EQ("0AIv7G8yEYAWHUk9123", resource->root_folder_id());
  EXPECT_EQ(5368709120LL, resource->quota_bytes_total());
  EXPECT_EQ(1073741824LL, resource->quota_bytes_used());
  EXPECT_EQ(8177LL, resource->largest_change_id());
}

TEST(DriveAPIParserTest, AboutResourceFromAccountMetadata) {
  AccountMetadata account_metadata;
  // Set up AccountMetadata instance.
  {
    account_metadata.set_quota_bytes_total(10000);
    account_metadata.set_quota_bytes_used(1000);
    account_metadata.set_largest_changestamp(100);
  }

  scoped_ptr<AboutResource> about_resource(
      AboutResource::CreateFromAccountMetadata(account_metadata,
                                               "dummy_root_id"));

  EXPECT_EQ(10000, about_resource->quota_bytes_total());
  EXPECT_EQ(1000, about_resource->quota_bytes_used());
  EXPECT_EQ(100, about_resource->largest_change_id());
  EXPECT_EQ("dummy_root_id", about_resource->root_folder_id());
}

// Test app list parsing.
TEST(DriveAPIParserTest, AppListParser) {
  std::string error;
  scoped_ptr<Value> document = test_util::LoadJSONFile("drive/applist.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<AppList> applist(new AppList);
  EXPECT_TRUE(applist->Parse(*document));

  EXPECT_EQ("\"Jm4BaSnCWNND-noZsHINRqj4ABC/tuqRBw0lvjUdPtc_2msA1tN4XYZ\"",
            applist->etag());
  ASSERT_EQ(2U, applist->items().size());
  // Check Drive app 1
  const AppResource& app1 = *applist->items()[0];
  EXPECT_EQ("123456788192", app1.application_id());
  EXPECT_EQ("Drive app 1", app1.name());
  EXPECT_EQ("", app1.object_type());
  EXPECT_TRUE(app1.supports_create());
  EXPECT_TRUE(app1.supports_import());
  EXPECT_TRUE(app1.is_installed());
  EXPECT_FALSE(app1.is_authorized());
  EXPECT_EQ("https://chrome.google.com/webstore/detail/"
            "abcdefghabcdefghabcdefghabcdefgh",
            app1.product_url().spec());

  ASSERT_EQ(1U, app1.primary_mimetypes().size());
  EXPECT_EQ("application/vnd.google-apps.drive-sdk.123456788192",
            *app1.primary_mimetypes()[0]);

  ASSERT_EQ(2U, app1.secondary_mimetypes().size());
  EXPECT_EQ("text/html", *app1.secondary_mimetypes()[0]);
  EXPECT_EQ("text/plain", *app1.secondary_mimetypes()[1]);

  ASSERT_EQ(2U, app1.primary_file_extensions().size());
  EXPECT_EQ("exe", *app1.primary_file_extensions()[0]);
  EXPECT_EQ("com", *app1.primary_file_extensions()[1]);

  EXPECT_EQ(0U, app1.secondary_file_extensions().size());

  ASSERT_EQ(6U, app1.icons().size());
  const DriveAppIcon& icon1 = *app1.icons()[0];
  EXPECT_EQ(DriveAppIcon::APPLICATION, icon1.category());
  EXPECT_EQ(10, icon1.icon_side_length());
  EXPECT_EQ("http://www.example.com/10.png", icon1.icon_url().spec());

  const DriveAppIcon& icon6 = *app1.icons()[5];
  EXPECT_EQ(DriveAppIcon::SHARED_DOCUMENT, icon6.category());
  EXPECT_EQ(16, icon6.icon_side_length());
  EXPECT_EQ("http://www.example.com/ds16.png", icon6.icon_url().spec());

  // Check Drive app 2
  const AppResource& app2 = *applist->items()[1];
  EXPECT_EQ("876543210000", app2.application_id());
  EXPECT_EQ("Drive app 2", app2.name());
  EXPECT_EQ("", app2.object_type());
  EXPECT_FALSE(app2.supports_create());
  EXPECT_FALSE(app2.supports_import());
  EXPECT_TRUE(app2.is_installed());
  EXPECT_FALSE(app2.is_authorized());
  EXPECT_EQ("https://chrome.google.com/webstore/detail/"
            "hgfedcbahgfedcbahgfedcbahgfedcba",
            app2.product_url().spec());

  ASSERT_EQ(3U, app2.primary_mimetypes().size());
  EXPECT_EQ("image/jpeg", *app2.primary_mimetypes()[0]);
  EXPECT_EQ("image/png", *app2.primary_mimetypes()[1]);
  EXPECT_EQ("application/vnd.google-apps.drive-sdk.876543210000",
            *app2.primary_mimetypes()[2]);

  EXPECT_EQ(0U, app2.secondary_mimetypes().size());
  EXPECT_EQ(0U, app2.primary_file_extensions().size());
  EXPECT_EQ(0U, app2.secondary_file_extensions().size());

  ASSERT_EQ(3U, app2.icons().size());
  const DriveAppIcon& icon2 = *app2.icons()[1];
  EXPECT_EQ(DriveAppIcon::DOCUMENT, icon2.category());
  EXPECT_EQ(10, icon2.icon_side_length());
  EXPECT_EQ("http://www.example.com/d10.png", icon2.icon_url().spec());
}

TEST(DriveAPIParserTest, AppListFromAccountMetadata) {
  AccountMetadata account_metadata;
  // Set up AccountMetadata instance.
  {
    ScopedVector<InstalledApp> installed_apps;
    scoped_ptr<InstalledApp> installed_app(new InstalledApp);
    installed_app->set_app_id("app_id");
    installed_app->set_app_name("name");
    installed_app->set_object_type("object_type");
    installed_app->set_supports_create(true);

    {
      ScopedVector<Link> links;
      scoped_ptr<Link> link(new Link);
      link->set_type(Link::LINK_PRODUCT);
      link->set_href(GURL("http://product/url"));
      links.push_back(link.release());
      installed_app->set_links(&links);
    }
    {
      ScopedVector<std::string> primary_mimetypes;
      primary_mimetypes.push_back(new std::string("primary_mimetype"));
      installed_app->set_primary_mimetypes(&primary_mimetypes);
    }
    {
      ScopedVector<std::string> secondary_mimetypes;
      secondary_mimetypes.push_back(new std::string("secondary_mimetype"));
      installed_app->set_secondary_mimetypes(&secondary_mimetypes);
    }
    {
      ScopedVector<std::string> primary_extensions;
      primary_extensions.push_back(new std::string("primary_extension"));
      installed_app->set_primary_extensions(&primary_extensions);
    }
    {
      ScopedVector<std::string> secondary_extensions;
      secondary_extensions.push_back(new std::string("secondary_extension"));
      installed_app->set_secondary_extensions(&secondary_extensions);
    }
    {
      ScopedVector<AppIcon> app_icons;
      scoped_ptr<AppIcon> app_icon(new AppIcon);
      app_icon->set_category(AppIcon::ICON_DOCUMENT);
      app_icon->set_icon_side_length(10);
      {
        ScopedVector<Link> links;
        scoped_ptr<Link> link(new Link);
        link->set_type(Link::LINK_ICON);
        link->set_href(GURL("http://icon/url"));
        links.push_back(link.release());
        app_icon->set_links(&links);
      }
      app_icons.push_back(app_icon.release());
      installed_app->set_app_icons(&app_icons);
    }

    installed_apps.push_back(installed_app.release());
    account_metadata.set_installed_apps(&installed_apps);
  }

  scoped_ptr<AppList> app_list(
      AppList::CreateFromAccountMetadata(account_metadata));
  const ScopedVector<AppResource>& items = app_list->items();
  ASSERT_EQ(1U, items.size());

  const AppResource& app_resource = *items[0];
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

  const ScopedVector<DriveAppIcon>& icons = app_resource.icons();
  ASSERT_EQ(1U, icons.size());
  const DriveAppIcon& icon = *icons[0];
  EXPECT_EQ(DriveAppIcon::DOCUMENT, icon.category());
  EXPECT_EQ(10, icon.icon_side_length());
  EXPECT_EQ("http://icon/url", icon.icon_url().spec());
}

// Test file list parsing.
TEST(DriveAPIParserTest, FileListParser) {
  std::string error;
  scoped_ptr<Value> document = test_util::LoadJSONFile("drive/filelist.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<FileList> filelist(new FileList);
  EXPECT_TRUE(filelist->Parse(*document));

  EXPECT_EQ("\"WtRjAPZWbDA7_fkFjc5ojsEvDEF/zyHTfoHpnRHovyi8bWpwK0DXABC\"",
            filelist->etag());
  EXPECT_EQ("EAIaggELEgA6egpi96It9mH_____f_8AAP__AAD_okhU-cHLz83KzszMxsjMzs_Ry"
            "NGJnridyrbHs7u9tv8AAP__AP7__n__AP8AokhU-cHLz83KzszMxsjMzs_RyNGJnr"
            "idyrbHs7u9tv8A__4QZCEiXPTi_wtIgTkAAAAAngnSXUgCDEAAIgsJPgart10AAAA"
            "ABC", filelist->next_page_token());
  EXPECT_EQ(GURL("https://www.googleapis.com/drive/v2/files?pageToken=EAIaggEL"
                 "EgA6egpi96It9mH_____f_8AAP__AAD_okhU-cHLz83KzszMxsjMzs_RyNGJ"
                 "nridyrbHs7u9tv8AAP__AP7__n__AP8AokhU-cHLz83KzszMxsjMzs_RyNGJ"
                 "nridyrbHs7u9tv8A__4QZCEiXPTi_wtIgTkAAAAAngnSXUgCDEAAIgsJPgar"
                 "t10AAAAABC"), filelist->next_link());

  ASSERT_EQ(3U, filelist->items().size());
  // Check file 1 (a regular file)
  const FileResource& file1 = *filelist->items()[0];
  EXPECT_EQ("0B4v7G8yEYAWHUmRrU2lMS2hLABC", file1.file_id());
  EXPECT_EQ("\"WtRjAPZWbDA7_fkFjc5ojsEvDEF/MTM0MzM2NzgwMDIXYZ\"",
            file1.etag());
  EXPECT_EQ("My first file data", file1.title());
  EXPECT_EQ("application/octet-stream", file1.mime_type());

  EXPECT_FALSE(file1.labels().is_starred());
  EXPECT_FALSE(file1.labels().is_hidden());
  EXPECT_FALSE(file1.labels().is_trashed());
  EXPECT_FALSE(file1.labels().is_restricted());
  EXPECT_TRUE(file1.labels().is_viewed());

  base::Time created_time;
  ASSERT_TRUE(util::GetTimeFromString("2012-07-24T08:51:16.570Z",
                                                   &created_time));
  EXPECT_EQ(created_time, file1.created_date());

  base::Time modified_time;
  ASSERT_TRUE(util::GetTimeFromString("2012-07-27T05:43:20.269Z",
                                                   &modified_time));
  EXPECT_EQ(modified_time, file1.modified_by_me_date());

  ASSERT_EQ(1U, file1.parents().size());
  EXPECT_EQ("0B4v7G8yEYAWHYW1OcExsUVZLABC", file1.parents()[0]->file_id());
  EXPECT_EQ(GURL("https://www.googleapis.com/drive/v2/files/"
                 "0B4v7G8yEYAWHYW1OcExsUVZLABC"),
            file1.parents()[0]->parent_link());
  EXPECT_FALSE(file1.parents()[0]->is_root());

  EXPECT_EQ(GURL("https://www.example.com/download"), file1.download_url());
  EXPECT_EQ("ext", file1.file_extension());
  EXPECT_EQ("d41d8cd98f00b204e9800998ecf8427e", file1.md5_checksum());
  EXPECT_EQ(1000U, file1.file_size());

  EXPECT_EQ(GURL("https://www.googleapis.com/drive/v2/files/"
                 "0B4v7G8yEYAWHUmRrU2lMS2hLABC"),
            file1.self_link());
  EXPECT_EQ(GURL("https://docs.google.com/file/d/"
                 "0B4v7G8yEYAWHUmRrU2lMS2hLABC/edit"),
            file1.alternate_link());
  EXPECT_EQ(GURL("https://docs.google.com/uc?"
                 "id=0B4v7G8yEYAWHUmRrU2lMS2hLABC&export=download"),
            file1.web_content_link());

  // Check file 2 (a Google Document)
  const FileResource& file2 = *filelist->items()[1];
  EXPECT_EQ("Test Google Document", file2.title());
  EXPECT_EQ("application/vnd.google-apps.document", file2.mime_type());

  EXPECT_TRUE(file2.labels().is_starred());
  EXPECT_TRUE(file2.labels().is_hidden());
  EXPECT_TRUE(file2.labels().is_trashed());
  EXPECT_TRUE(file2.labels().is_restricted());
  EXPECT_TRUE(file2.labels().is_viewed());

  EXPECT_EQ(0U, file2.file_size());

  ASSERT_EQ(0U, file2.parents().size());

  EXPECT_EQ(GURL("https://docs.google.com/a/chromium.org/document/d/"
                 "1Pc8jzfU1ErbN_eucMMqdqzY3eBm0v8sxXm_1CtLxABC/preview"),
            file2.embed_link());
  EXPECT_EQ(GURL("https://docs.google.com/feeds/vt?gd=true&"
                 "id=1Pc8jzfU1ErbN_eucMMqdqzY3eBm0v8sxXm_1CtLxABC&"
                 "v=3&s=AMedNnoAAAAAUBJyB0g8HbxZaLRnlztxefZPS24LiXYZ&sz=s220"),
            file2.thumbnail_link());

  // Check file 3 (a folder)
  const FileResource& file3 = *filelist->items()[2];
  EXPECT_EQ(0U, file3.file_size());
  EXPECT_EQ("TestFolder", file3.title());
  EXPECT_EQ("application/vnd.google-apps.folder", file3.mime_type());
  ASSERT_TRUE(file3.IsDirectory());

  ASSERT_EQ(1U, file3.parents().size());
  EXPECT_EQ("0AIv7G8yEYAWHUk9ABC", file3.parents()[0]->file_id());
  EXPECT_TRUE(file3.parents()[0]->is_root());
}

// Test change list parsing.
TEST(DriveAPIParserTest, ChangeListParser) {
  std::string error;
  scoped_ptr<Value> document =
      test_util::LoadJSONFile("drive/changelist.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<ChangeList> changelist(new ChangeList);
  EXPECT_TRUE(changelist->Parse(*document));

  EXPECT_EQ("\"Lp2bjAtLP341hvGmYHhxjYyBPJ8/BWbu_eylt5f_aGtCN6mGRv9hABC\"",
            changelist->etag());
  EXPECT_EQ("8929", changelist->next_page_token());
  EXPECT_EQ("https://www.googleapis.com/drive/v2/changes?pageToken=8929",
            changelist->next_link().spec());
  EXPECT_EQ(13664, changelist->largest_change_id());

  ASSERT_EQ(4U, changelist->items().size());

  const ChangeResource& change1 = *changelist->items()[0];
  EXPECT_EQ(8421, change1.change_id());
  EXPECT_FALSE(change1.is_deleted());
  EXPECT_EQ("1Pc8jzfU1ErbN_eucMMqdqzY3eBm0v8sxXm_1CtLxABC", change1.file_id());
  EXPECT_EQ(change1.file_id(), change1.file().file_id());

  scoped_ptr<ResourceEntry> entry1(
      ResourceEntry::CreateFromChangeResource(change1));
  EXPECT_EQ(change1.file_id(), entry1->resource_id());
  EXPECT_EQ(change1.is_deleted(), entry1->deleted());

  const ChangeResource& change2 = *changelist->items()[1];
  EXPECT_EQ(8424, change2.change_id());
  EXPECT_FALSE(change2.is_deleted());
  EXPECT_EQ("0B4v7G8yEYAWHUmRrU2lMS2hLABC", change2.file_id());
  EXPECT_EQ(change2.file_id(), change2.file().file_id());

  scoped_ptr<ResourceEntry> entry2(
      ResourceEntry::CreateFromChangeResource(change2));
  EXPECT_EQ(change2.file_id(), entry2->resource_id());
  EXPECT_EQ(change2.is_deleted(), entry2->deleted());

  const ChangeResource& change3 = *changelist->items()[2];
  EXPECT_EQ(8429, change3.change_id());
  EXPECT_FALSE(change3.is_deleted());
  EXPECT_EQ("0B4v7G8yEYAWHYW1OcExsUVZLABC", change3.file_id());
  EXPECT_EQ(change3.file_id(), change3.file().file_id());

  scoped_ptr<ResourceEntry> entry3(
      ResourceEntry::CreateFromChangeResource(change3));
  EXPECT_EQ(change3.file_id(), entry3->resource_id());
  EXPECT_EQ(change3.is_deleted(), entry3->deleted());

  // Deleted entry.
  const ChangeResource& change4 = *changelist->items()[3];
  EXPECT_EQ(8430, change4.change_id());
  EXPECT_EQ("ABCv7G8yEYAWHc3Y5X0hMSkJYXYZ", change4.file_id());
  EXPECT_TRUE(change4.is_deleted());

  scoped_ptr<ResourceEntry> entry4(
      ResourceEntry::CreateFromChangeResource(change4));
  EXPECT_EQ(change4.file_id(), entry4->resource_id());
  EXPECT_EQ(change4.is_deleted(), entry4->deleted());
}
#endif  // OS_CHROMEOS

}  // namespace google_apis
