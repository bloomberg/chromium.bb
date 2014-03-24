// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace extensions {

class ExternalPolicyLoaderTest : public testing::Test {
 public:
  ExternalPolicyLoaderTest() : ui_thread_(BrowserThread::UI, &loop_) {
  }

  virtual ~ExternalPolicyLoaderTest() {}

 private:
  // We need these to satisfy BrowserThread::CurrentlyOn(BrowserThread::UI)
  // checks in ExternalProviderImpl.
  base::MessageLoopForIO loop_;
  content::TestBrowserThread ui_thread_;
};

class MockExternalPolicyProviderVisitor
    : public ExternalProviderInterface::VisitorInterface {
 public:
  MockExternalPolicyProviderVisitor() {
  }

  // Initialize a provider with |policy_forcelist|, and check that it installs
  // exactly the extensions specified in |expected_extensions|.
  void Visit(const base::DictionaryValue& policy_forcelist,
             const std::set<std::string>& expected_extensions) {
    profile_.reset(new TestingProfile);
    profile_->GetTestingPrefService()->SetManagedPref(
        pref_names::kInstallForceList, policy_forcelist.DeepCopy());
    provider_.reset(new ExternalProviderImpl(
        this,
        new ExternalPolicyLoader(profile_.get()),
        profile_.get(),
        Manifest::INVALID_LOCATION,
        Manifest::EXTERNAL_POLICY_DOWNLOAD,
        Extension::NO_FLAGS));

    // Extensions will be removed from this list as they visited,
    // so it should be emptied by the end.
    expected_extensions_ = expected_extensions;
    provider_->VisitRegisteredExtension();
    EXPECT_TRUE(expected_extensions_.empty());
  }

  virtual bool OnExternalExtensionFileFound(const std::string& id,
                                            const Version* version,
                                            const base::FilePath& path,
                                            Manifest::Location unused,
                                            int unused2,
                                            bool unused3) OVERRIDE {
    ADD_FAILURE() << "There should be no external extensions from files.";
    return false;
  }

  virtual bool OnExternalExtensionUpdateUrlFound(
      const std::string& id,
      const std::string& install_parameter,
      const GURL& update_url,
      Manifest::Location location,
      int unused1,
      bool unused2) OVERRIDE {
    // Extension has the correct location.
    EXPECT_EQ(Manifest::EXTERNAL_POLICY_DOWNLOAD, location);

    // Provider returns the correct location when asked.
    Manifest::Location location1;
    scoped_ptr<Version> version1;
    provider_->GetExtensionDetails(id, &location1, &version1);
    EXPECT_EQ(Manifest::EXTERNAL_POLICY_DOWNLOAD, location1);
    EXPECT_FALSE(version1.get());

    // Remove the extension from our list.
    EXPECT_EQ(1U, expected_extensions_.erase(id));
    return true;
  }

  virtual void OnExternalProviderReady(
      const ExternalProviderInterface* provider) OVERRIDE {
    EXPECT_EQ(provider, provider_.get());
    EXPECT_TRUE(provider->IsReady());
  }

 private:
  std::set<std::string> expected_extensions_;

  scoped_ptr<TestingProfile> profile_;

  scoped_ptr<ExternalProviderImpl> provider_;

  DISALLOW_COPY_AND_ASSIGN(MockExternalPolicyProviderVisitor);
};

TEST_F(ExternalPolicyLoaderTest, PolicyIsParsed) {
  base::DictionaryValue forced_extensions;
  std::set<std::string> expected_extensions;
  extensions::ExternalPolicyLoader::AddExtension(
      &forced_extensions, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "http://www.example.com/crx?a=5;b=6");
  expected_extensions.insert("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  extensions::ExternalPolicyLoader::AddExtension(
      &forced_extensions, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      "https://clients2.google.com/service/update2/crx");
  expected_extensions.insert("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

  MockExternalPolicyProviderVisitor mv;
  mv.Visit(forced_extensions, expected_extensions);
}

TEST_F(ExternalPolicyLoaderTest, InvalidEntriesIgnored) {
  base::DictionaryValue forced_extensions;
  std::set<std::string> expected_extensions;

  extensions::ExternalPolicyLoader::AddExtension(
      &forced_extensions, "cccccccccccccccccccccccccccccccc",
      "http://www.example.com/crx");
  expected_extensions.insert("cccccccccccccccccccccccccccccccc");

  // Add invalid entries.
  forced_extensions.SetString("invalid", "http://www.example.com/crx");
  forced_extensions.SetString("dddddddddddddddddddddddddddddddd",
                              std::string());
  forced_extensions.SetString("invalid", "bad");

  MockExternalPolicyProviderVisitor mv;
  mv.Visit(forced_extensions, expected_extensions);
}

}  // namespace extensions
