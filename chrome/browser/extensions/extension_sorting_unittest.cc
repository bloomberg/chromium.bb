// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_prefs_unittest.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/common/extensions/extension_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_manifest_keys;

class ExtensionSortingTest : public ExtensionPrefsTest {
 protected:
  ExtensionSorting* extension_sorting() {
    return prefs()->extension_sorting();
  }
};

class ExtensionSortingAppLocation : public ExtensionSortingTest {
 public:
  virtual void Initialize() OVERRIDE {
    extension_ = prefs_.AddExtension("not_an_app");
    // Non-apps should not have any app launch ordinal or page ordinal.
    prefs()->OnExtensionInstalled(extension_.get(), Extension::ENABLED,
                                  false, StringOrdinal());
  }

  virtual void Verify() OVERRIDE {
    EXPECT_FALSE(
        extension_sorting()->GetAppLaunchOrdinal(extension_->id()).IsValid());
    EXPECT_FALSE(
        extension_sorting()->GetPageOrdinal(extension_->id()).IsValid());
  }

 private:
  scoped_refptr<Extension> extension_;
};
TEST_F(ExtensionSortingAppLocation, ExtensionSortingAppLocation) {}

class ExtensionSortingAppLaunchOrdinal : public ExtensionSortingTest {
 public:
  virtual void Initialize() OVERRIDE {
    // No extensions yet.
    StringOrdinal page = StringOrdinal::CreateInitialOrdinal();
    EXPECT_TRUE(StringOrdinal::CreateInitialOrdinal().Equal(
        extension_sorting()->CreateNextAppLaunchOrdinal(page)));

    extension_ = prefs_.AddApp("on_extension_installed");
    EXPECT_FALSE(prefs()->IsExtensionDisabled(extension_->id()));
    prefs()->OnExtensionInstalled(extension_.get(), Extension::ENABLED,
                                  false, StringOrdinal());
  }

  virtual void Verify() OVERRIDE {
    StringOrdinal launch_ordinal =
        extension_sorting()->GetAppLaunchOrdinal(extension_->id());
    StringOrdinal page_ordinal = StringOrdinal::CreateInitialOrdinal();

    // Extension should have been assigned a valid StringOrdinal.
    EXPECT_TRUE(launch_ordinal.IsValid());
    EXPECT_TRUE(launch_ordinal.LessThan(
        extension_sorting()->CreateNextAppLaunchOrdinal(page_ordinal)));
    // Set a new launch ordinal of and verify it comes after.
    extension_sorting()->SetAppLaunchOrdinal(
        extension_->id(),
        extension_sorting()->CreateNextAppLaunchOrdinal(page_ordinal));
    StringOrdinal new_launch_ordinal =
        extension_sorting()->GetAppLaunchOrdinal(extension_->id());
    EXPECT_TRUE(launch_ordinal.LessThan(new_launch_ordinal));

    // This extension doesn't exist, so it should return an invalid
    // StringOrdinal.
    StringOrdinal invalid_app_launch_ordinal =
        extension_sorting()->GetAppLaunchOrdinal("foo");
    EXPECT_FALSE(invalid_app_launch_ordinal.IsValid());
    EXPECT_EQ(-1, extension_sorting()->PageStringOrdinalAsInteger(
        invalid_app_launch_ordinal));

    // The second page doesn't have any apps so its next launch ordinal should
    // be the first launch ordinal.
    StringOrdinal next_page = page_ordinal.CreateAfter();
    StringOrdinal next_page_app_launch_ordinal =
        extension_sorting()->CreateNextAppLaunchOrdinal(next_page);
    EXPECT_TRUE(next_page_app_launch_ordinal.Equal(
        extension_sorting()->CreateFirstAppLaunchOrdinal(next_page)));
  }

 private:
  scoped_refptr<Extension> extension_;
};
TEST_F(ExtensionSortingAppLaunchOrdinal, ExtensionSortingAppLaunchOrdinal) {}

class ExtensionSortingPageOrdinal : public ExtensionSortingTest {
 public:
  virtual void Initialize() OVERRIDE{
    extension_ = prefs_.AddApp("page_ordinal");
    // Install with a page preference.
    first_page_ = StringOrdinal::CreateInitialOrdinal();
    prefs()->OnExtensionInstalled(extension_.get(), Extension::ENABLED,
                                  false, first_page_);
    EXPECT_TRUE(first_page_.Equal(
        extension_sorting()->GetPageOrdinal(extension_->id())));
    EXPECT_EQ(0, extension_sorting()->PageStringOrdinalAsInteger(first_page_));

    scoped_refptr<Extension> extension2 = prefs_.AddApp("page_ordinal_2");
    // Install without any page preference.
    prefs()->OnExtensionInstalled(extension2.get(), Extension::ENABLED,
                                  false, StringOrdinal());
    EXPECT_TRUE(first_page_.Equal(
        extension_sorting()->GetPageOrdinal(extension2->id())));
  }
  virtual void Verify() OVERRIDE {
    // Set the page ordinal.
    StringOrdinal new_page = first_page_.CreateAfter();
    extension_sorting()->SetPageOrdinal(extension_->id(), new_page);
    // Verify the page ordinal.
    EXPECT_TRUE(
        new_page.Equal(extension_sorting()->GetPageOrdinal(extension_->id())));
    EXPECT_EQ(1, extension_sorting()->PageStringOrdinalAsInteger(new_page));

    // This extension doesn't exist, so it should return an invalid
    // StringOrdinal.
    EXPECT_FALSE(extension_sorting()->GetPageOrdinal("foo").IsValid());
  }

 private:
  StringOrdinal first_page_;
  scoped_refptr<Extension> extension_;
};
TEST_F(ExtensionSortingPageOrdinal, ExtensionSortingPageOrdinal) {}

// Ensure that ExtensionSorting is able to properly initialize off a set
// of old page and app launch indices and properly convert them.
class ExtensionSortingInitialize
    : public ExtensionPrefsPrepopulatedTest {
 public:
  ExtensionSortingInitialize() {}
  virtual ~ExtensionSortingInitialize() {}

  virtual void Initialize() OVERRIDE {
    // A preference determining the order of which the apps appear on the NTP.
    const char kPrefAppLaunchIndexDeprecated[] = "app_launcher_index";
    // A preference determining the page on which an app appears in the NTP.
    const char kPrefPageIndexDeprecated[] = "page_index";

    // Setup the deprecated preferences.
    ExtensionScopedPrefs* scoped_prefs =
        static_cast<ExtensionScopedPrefs*>(prefs());
    scoped_prefs->UpdateExtensionPref(ext1_->id(),
                                       kPrefAppLaunchIndexDeprecated,
                                       Value::CreateIntegerValue(0));
    scoped_prefs->UpdateExtensionPref(ext1_->id(),
                                       kPrefPageIndexDeprecated,
                                       Value::CreateIntegerValue(0));

    scoped_prefs->UpdateExtensionPref(ext2_->id(),
                                       kPrefAppLaunchIndexDeprecated,
                                       Value::CreateIntegerValue(1));
    scoped_prefs->UpdateExtensionPref(ext2_->id(),
                                       kPrefPageIndexDeprecated,
                                       Value::CreateIntegerValue(0));

    scoped_prefs->UpdateExtensionPref(ext3_->id(),
                                      kPrefAppLaunchIndexDeprecated,
                                      Value::CreateIntegerValue(0));
    scoped_prefs->UpdateExtensionPref(ext3_->id(),
                                      kPrefPageIndexDeprecated,
                                      Value::CreateIntegerValue(1));

    // We insert the ids in reserve order so that we have to deal with the
    // element on the 2nd page before the 1st page is seen.
    ExtensionPrefs::ExtensionIdSet ids;
    ids.push_back(ext3_->id());
    ids.push_back(ext2_->id());
    ids.push_back(ext1_->id());

    prefs()->extension_sorting()->Initialize(ids);
  }
  virtual void Verify() OVERRIDE {
    StringOrdinal first_ordinal = StringOrdinal::CreateInitialOrdinal();
    ExtensionSorting* extension_sorting = prefs()->extension_sorting();

    EXPECT_TRUE(first_ordinal.Equal(
        extension_sorting->GetAppLaunchOrdinal(ext1_->id())));
    EXPECT_TRUE(first_ordinal.LessThan(
        extension_sorting->GetAppLaunchOrdinal(ext2_->id())));
    EXPECT_TRUE(first_ordinal.Equal(
        extension_sorting->GetAppLaunchOrdinal(ext3_->id())));

    EXPECT_TRUE(first_ordinal.Equal(
        extension_sorting->GetPageOrdinal(ext1_->id())));
    EXPECT_TRUE(first_ordinal.Equal(
        extension_sorting->GetPageOrdinal(ext2_->id())));
    EXPECT_TRUE(first_ordinal.LessThan(
        extension_sorting->GetPageOrdinal(ext3_->id())));
  }
};
TEST_F(ExtensionSortingInitialize, ExtensionSortingInitialize) {}

// Make sure that initialization still works when no extensions are present
// (i.e. make sure that the web store icon is still loaded into the map).
class ExtensionSortingInitializeWithNoApps
    : public ExtensionPrefsPrepopulatedTest {
 public:
  ExtensionSortingInitializeWithNoApps() {}
  virtual ~ExtensionSortingInitializeWithNoApps() {}

  virtual void Initialize() OVERRIDE {
    ExtensionSorting* extension_sorting = prefs()->extension_sorting();

    // Make sure that the web store has valid ordinals.
    StringOrdinal initial_ordinal = StringOrdinal::CreateInitialOrdinal();
    extension_sorting->SetPageOrdinal(extension_misc::kWebStoreAppId,
                                      initial_ordinal);
    extension_sorting->SetAppLaunchOrdinal(extension_misc::kWebStoreAppId,
                                           initial_ordinal);

    ExtensionPrefs::ExtensionIdSet ids;
    extension_sorting->Initialize(ids);
  }
  virtual void Verify() OVERRIDE {
    ExtensionSorting* extension_sorting = prefs()->extension_sorting();

    StringOrdinal page =
        extension_sorting->GetPageOrdinal(extension_misc::kWebStoreAppId);
    EXPECT_TRUE(page.IsValid());

    ExtensionSorting::PageOrdinalMap::iterator page_it =
        extension_sorting->ntp_ordinal_map_.find(page);
    EXPECT_TRUE(page_it != extension_sorting->ntp_ordinal_map_.end());

    StringOrdinal app_launch =
        extension_sorting->GetPageOrdinal(extension_misc::kWebStoreAppId);
    EXPECT_TRUE(app_launch.IsValid());

    ExtensionSorting::AppLaunchOrdinalMap::iterator app_launch_it =
        page_it->second.find(app_launch);
    EXPECT_TRUE(app_launch_it != page_it->second.end());
  }
};
TEST_F(ExtensionSortingInitializeWithNoApps,
       ExtensionSortingInitializeWithNoApps) {}

// Tests the application index to ordinal migration code for values that
// shouldn't be converted. This should be removed when the migrate code
// is taken out.
// http://crbug.com/107376
class ExtensionSortingMigrateAppIndexInvalid
 : public ExtensionPrefsPrepopulatedTest {
 public:
  ExtensionSortingMigrateAppIndexInvalid() {}
  virtual ~ExtensionSortingMigrateAppIndexInvalid() {}

  virtual void Initialize() OVERRIDE {
    // A preference determining the order of which the apps appear on the NTP.
    const char kPrefAppLaunchIndexDeprecated[] = "app_launcher_index";
    // A preference determining the page on which an app appears in the NTP.
    const char kPrefPageIndexDeprecated[] = "page_index";

    // Setup the deprecated preference.
    ExtensionScopedPrefs* scoped_prefs =
        static_cast<ExtensionScopedPrefs*>(prefs());
    scoped_prefs->UpdateExtensionPref(ext1_->id(),
                                      kPrefAppLaunchIndexDeprecated,
                                      Value::CreateIntegerValue(0));
    scoped_prefs->UpdateExtensionPref(ext1_->id(),
                                      kPrefPageIndexDeprecated,
                                      Value::CreateIntegerValue(-1));

    ExtensionPrefs::ExtensionIdSet ids;
    ids.push_back(ext1_->id());

    prefs()->extension_sorting()->Initialize(ids);
  }
  virtual void Verify() OVERRIDE {
    // Make sure that the invalid page_index wasn't converted over.
    EXPECT_FALSE(prefs()->extension_sorting()->GetAppLaunchOrdinal(
        ext1_->id()).IsValid());
  }
};
TEST_F(ExtensionSortingMigrateAppIndexInvalid,
       ExtensionSortingMigrateAppIndexInvalid) {}

class ExtensionSortingPageOrdinalMapping :
    public ExtensionPrefsPrepopulatedTest {
 public:
  ExtensionSortingPageOrdinalMapping() {}
  virtual ~ExtensionSortingPageOrdinalMapping() {}
  virtual void Initialize() {}

  virtual void Verify() {
    std::string ext_1 = "ext_1";
    std::string ext_2 = "ext_2";

    ExtensionSorting* extension_sorting = prefs()->extension_sorting();
    StringOrdinal first_ordinal = StringOrdinal::CreateInitialOrdinal();

    // Ensure attempting to removing a mapping with an invalid page doesn't
    // modify the map.
    EXPECT_TRUE(extension_sorting->ntp_ordinal_map_.empty());
    extension_sorting->RemoveOrdinalMapping(
        ext_1, first_ordinal, first_ordinal);
    EXPECT_TRUE(extension_sorting->ntp_ordinal_map_.empty());

    // Add new mappings.
    extension_sorting->AddOrdinalMapping(ext_1, first_ordinal, first_ordinal);
    extension_sorting->AddOrdinalMapping(ext_2, first_ordinal, first_ordinal);

    EXPECT_EQ(1U, extension_sorting->ntp_ordinal_map_.size());
    EXPECT_EQ(2U, extension_sorting->ntp_ordinal_map_[first_ordinal].size());

    ExtensionSorting::AppLaunchOrdinalMap::iterator it =
        extension_sorting->ntp_ordinal_map_[first_ordinal].find(first_ordinal);
    EXPECT_EQ(ext_1, it->second);
    ++it;
    EXPECT_EQ(ext_2, it->second);

    extension_sorting->RemoveOrdinalMapping(ext_1,
                                            first_ordinal,
                                            first_ordinal);
    EXPECT_EQ(1U, extension_sorting->ntp_ordinal_map_.size());
    EXPECT_EQ(1U, extension_sorting->ntp_ordinal_map_[first_ordinal].size());

    it = extension_sorting->ntp_ordinal_map_[first_ordinal].find(
        first_ordinal);
    EXPECT_EQ(ext_2, it->second);

    // Ensure that attempting to remove an extension with a valid page and app
    // launch ordinals, but a unused id has no effect.
    extension_sorting->RemoveOrdinalMapping(
        "invalid_ext", first_ordinal, first_ordinal);
    EXPECT_EQ(1U, extension_sorting->ntp_ordinal_map_.size());
    EXPECT_EQ(1U, extension_sorting->ntp_ordinal_map_[first_ordinal].size());

    it = extension_sorting->ntp_ordinal_map_[first_ordinal].find(
        first_ordinal);
    EXPECT_EQ(ext_2, it->second);
  }
};
TEST_F(ExtensionSortingPageOrdinalMapping,
       ExtensionSortingPageOrdinalMapping) {}

class ExtensionSortingPreinstalledAppsBase :
    public ExtensionPrefsPrepopulatedTest {
 public:
  ExtensionSortingPreinstalledAppsBase() {
    DictionaryValue simple_dict;
    simple_dict.SetString(keys::kVersion, "1.0.0.0");
    simple_dict.SetString(keys::kName, "unused");
    simple_dict.SetString(keys::kApp, "true");
    simple_dict.SetString(keys::kLaunchLocalPath, "fake.html");

    std::string error;
    app1_scoped_ = Extension::Create(
        prefs_.temp_dir().AppendASCII("app1_"), Extension::EXTERNAL_PREF,
        simple_dict, Extension::STRICT_ERROR_CHECKS, &error);
    prefs()->OnExtensionInstalled(app1_scoped_.get(),
                                  Extension::ENABLED,
                                  false,
                                  StringOrdinal());

    app2_scoped_ = Extension::Create(
        prefs_.temp_dir().AppendASCII("app2_"), Extension::EXTERNAL_PREF,
        simple_dict, Extension::STRICT_ERROR_CHECKS, &error);
    prefs()->OnExtensionInstalled(app2_scoped_.get(),
                                  Extension::ENABLED,
                                  false,
                                  StringOrdinal());

    app1_ = app1_scoped_.get();
    app2_ = app2_scoped_.get();
  }
  virtual ~ExtensionSortingPreinstalledAppsBase() {}

 protected:
  // Weak references, for convenience.
  Extension* app1_;
  Extension* app2_;

 private:
    scoped_refptr<Extension> app1_scoped_;
    scoped_refptr<Extension> app2_scoped_;
};

class ExtensionSortingGetMinOrMaxAppLaunchOrdinalsOnPage :
    public ExtensionSortingPreinstalledAppsBase {
 public:
  ExtensionSortingGetMinOrMaxAppLaunchOrdinalsOnPage() {}
  virtual ~ExtensionSortingGetMinOrMaxAppLaunchOrdinalsOnPage() {}

  virtual void Initialize() OVERRIDE {}
  virtual void Verify() OVERRIDE {
    StringOrdinal page = StringOrdinal::CreateInitialOrdinal();
    ExtensionSorting* extension_sorting = prefs()->extension_sorting();

    StringOrdinal min = extension_sorting->GetMinOrMaxAppLaunchOrdinalsOnPage(
        page,
        ExtensionSorting::MIN_ORDINAL);
    StringOrdinal max = extension_sorting->GetMinOrMaxAppLaunchOrdinalsOnPage(
        page,
        ExtensionSorting::MAX_ORDINAL);
    EXPECT_TRUE(min.IsValid());
    EXPECT_TRUE(max.IsValid());
    EXPECT_TRUE(min.LessThan(max));

    // Ensure that the min and max values aren't set for empty pages.
    min = StringOrdinal();
    max = StringOrdinal();
    StringOrdinal empty_page = page.CreateAfter();
    EXPECT_FALSE(min.IsValid());
    EXPECT_FALSE(max.IsValid());
    min = extension_sorting->GetMinOrMaxAppLaunchOrdinalsOnPage(
        empty_page,
        ExtensionSorting::MIN_ORDINAL);
    max = extension_sorting->GetMinOrMaxAppLaunchOrdinalsOnPage(
        empty_page,
        ExtensionSorting::MAX_ORDINAL);
    EXPECT_FALSE(min.IsValid());
    EXPECT_FALSE(max.IsValid());
  }
};
TEST_F(ExtensionSortingGetMinOrMaxAppLaunchOrdinalsOnPage,
       ExtensionSortingGetMinOrMaxAppLaunchOrdinalsOnPage) {}

// Make sure that empty pages aren't removes from the integer to ordinal
// mapping. See http://www.crbug.com/109802 for details.
class ExtensionSortingKeepEmptyStringOrdinalPages :
    public ExtensionSortingPreinstalledAppsBase {
 public:
  ExtensionSortingKeepEmptyStringOrdinalPages() {}
  virtual ~ExtensionSortingKeepEmptyStringOrdinalPages() {}

  virtual void Initialize() {
    ExtensionSorting* extension_sorting = prefs()->extension_sorting();

    StringOrdinal first_page = StringOrdinal::CreateInitialOrdinal();
    extension_sorting->SetPageOrdinal(app1_->id(), first_page);
    EXPECT_EQ(0, extension_sorting->PageStringOrdinalAsInteger(first_page));

    last_page_ = first_page.CreateAfter();
    extension_sorting->SetPageOrdinal(app2_->id(), last_page_);
    EXPECT_EQ(1, extension_sorting->PageStringOrdinalAsInteger(last_page_));

    // Move the second app to create an empty page.
    extension_sorting->SetPageOrdinal(app2_->id(), first_page);
    EXPECT_EQ(0, extension_sorting->PageStringOrdinalAsInteger(first_page));
  }
  virtual void Verify() {
    ExtensionSorting* extension_sorting = prefs()->extension_sorting();

    // Move the second app to a new empty page at the end, skipping over
    // the current empty page.
    last_page_ = last_page_.CreateAfter();
    extension_sorting->SetPageOrdinal(app2_->id(), last_page_);
    EXPECT_EQ(2, extension_sorting->PageStringOrdinalAsInteger(last_page_));
    EXPECT_TRUE(
        last_page_.Equal(extension_sorting->PageIntegerAsStringOrdinal(2)));
  }

 private:
  StringOrdinal last_page_;
};
TEST_F(ExtensionSortingKeepEmptyStringOrdinalPages,
       ExtensionSortingKeepEmptyStringOrdinalPages) {}
