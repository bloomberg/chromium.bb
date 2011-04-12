// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_selection_policy.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_temp_dir.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using std::string;
using std::vector;

#if !defined(OS_CHROMEOS)
#error This file is meant to be compiled on ChromeOS only.
#endif

namespace chromeos {

const char kBasicPolicy[] = "# This is a basic policy\n"
                            "plugin test.so\n"
                            "allow foo.com\n"
                            "deny bar.com\n";

const char kNoPluginPolicy[] = "# This is a policy with missing plugin.\n"
                               "# Missing plugin test.so\n"
                               "allow foo.com\n"
                               "deny bar.com\n";
const char kNoRulesPolicy[] = "# This is a policy with no rules\n"
                              "plugin test.so\n";

const char kEmptyPolicy[] = "# This is an empty policy\n";

const char kGlobalPolicy[] = "# This is a test with global deny/allow\n"
                             "plugin test.so\n"
                             "deny\n"
                             "plugin test1.so\n"
                             "allow\n";

const char kCommentTestPolicy[] = "# This is a policy with inline comments.\n"
                                  "plugin test.so# like this\n"
                                  "allow foo.com # and this\n"
                                  "deny bar.com  # and this\n";

const char kMultiPluginTestPolicy[] =
    "# This is a policy with multiple plugins.\n"
    "plugin allow_foo.so\n"
    "allow foo.com\n"
    "deny bar.com\n"
    "plugin allow_baz_bim1.so\n"
    "deny google.com\n"
    "allow baz.com\n"
    "allow bim.com\n"
    "plugin allow_baz_bim2.so\n"
    "deny google.com\n"
    "allow baz.com\n"
    "allow bim.com\n";

const char kWhitespaceTestPolicy[] = "# This is a policy with odd whitespace.\n"
                                     "  plugin\ttest.so# like this\n"
                                     "\n\n \n   allow\t\tfoo.com # and this\n"
                                     "\tdeny     bar.com\t\t\t# and this    \n";

class PluginSelectionPolicyTest : public PlatformTest {
 public:
  PluginSelectionPolicyTest()
      : loop_(MessageLoop::TYPE_DEFAULT),
        file_thread_(BrowserThread::FILE, &loop_) {}

  virtual void SetUp() {
    PlatformTest::SetUp();
    // Create a policy file to test with.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  bool CreatePolicy(const std::string& name,
                    const std::string& contents,
                    FilePath* path) {
    FilePath policy_file = GetPolicyPath(name);
    size_t bytes_written = file_util::WriteFile(policy_file,
                                                contents.c_str(),
                                                contents.size());
    if (path)
      *path = policy_file;

    return bytes_written == contents.size();
  }

  FilePath GetPolicyPath(const std::string& name) {
    FilePath policy_file(temp_dir_.path());
    return policy_file.Append(FilePath(name));
  }

 private:
  ScopedTempDir temp_dir_;
  MessageLoop loop_;
  BrowserThread file_thread_;
};

TEST_F(PluginSelectionPolicyTest, Basic) {
  FilePath path;
  ASSERT_TRUE(CreatePolicy("basic", kBasicPolicy, &path));
  scoped_refptr<PluginSelectionPolicy> policy = new PluginSelectionPolicy;
  EXPECT_TRUE(policy->InitFromFile(path));
}

TEST_F(PluginSelectionPolicyTest, InitFromFile) {
  {
    FilePath path;
    ASSERT_TRUE(CreatePolicy("basic", kBasicPolicy, &path));
    scoped_refptr<PluginSelectionPolicy> policy = new PluginSelectionPolicy;
    EXPECT_TRUE(policy->InitFromFile(path));
  }

  {
    FilePath path;
    ASSERT_TRUE(CreatePolicy("no_plugin", kNoPluginPolicy, &path));
    scoped_refptr<PluginSelectionPolicy> policy = new PluginSelectionPolicy;
    EXPECT_FALSE(policy->InitFromFile(path));
  }

  {
    FilePath path;
    ASSERT_TRUE(CreatePolicy("no_rules", kNoRulesPolicy, &path));
    scoped_refptr<PluginSelectionPolicy> policy = new PluginSelectionPolicy;
    EXPECT_TRUE(policy->InitFromFile(path));
  }

  {
    FilePath path;
    ASSERT_TRUE(CreatePolicy("empty", kEmptyPolicy, &path));
    scoped_refptr<PluginSelectionPolicy> policy = new PluginSelectionPolicy;
    EXPECT_TRUE(policy->InitFromFile(path));
  }

  {
    FilePath path;
    ASSERT_TRUE(CreatePolicy("global", kGlobalPolicy, &path));
    scoped_refptr<PluginSelectionPolicy> policy = new PluginSelectionPolicy;
    EXPECT_TRUE(policy->InitFromFile(path));
  }

  {
    FilePath path;
    ASSERT_TRUE(CreatePolicy("comment", kCommentTestPolicy, &path));
    scoped_refptr<PluginSelectionPolicy> policy = new PluginSelectionPolicy;
    EXPECT_TRUE(policy->InitFromFile(path));
  }

  {
    FilePath path;
    ASSERT_TRUE(CreatePolicy("multi_plugin", kMultiPluginTestPolicy, &path));
    scoped_refptr<PluginSelectionPolicy> policy = new PluginSelectionPolicy;
    EXPECT_TRUE(policy->InitFromFile(path));
  }

  {
    FilePath path;
    ASSERT_TRUE(CreatePolicy("whitespace", kWhitespaceTestPolicy, &path));
    scoped_refptr<PluginSelectionPolicy> policy = new PluginSelectionPolicy;
    EXPECT_TRUE(policy->InitFromFile(path));
  }
}

TEST_F(PluginSelectionPolicyTest, IsAllowed) {
  FilePath path;
  ASSERT_TRUE(CreatePolicy("basic", kBasicPolicy, &path));

  scoped_refptr<PluginSelectionPolicy> policy1 = new PluginSelectionPolicy;
  ASSERT_TRUE(policy1->InitFromFile(path));
  EXPECT_TRUE(policy1->IsAllowed(GURL("http://www.foo.com/blah.html"),
                                 FilePath("/usr/local/bin/test.so")));
  EXPECT_FALSE(policy1->IsAllowed(GURL("http://www.bar.com/blah.html"),
                                  FilePath("/usr/local/bin/test.so")));
  EXPECT_FALSE(policy1->IsAllowed(GURL("http://www.baz.com/blah.html"),
                                  FilePath("/usr/local/bin/test.so")));
  EXPECT_TRUE(policy1->IsAllowed(GURL("http://www.baz.com/blah.html"),
                                 FilePath("/usr/local/bin/real.so")));

  scoped_refptr<PluginSelectionPolicy> policy2 = new PluginSelectionPolicy;
  ASSERT_TRUE(CreatePolicy("no_rules", kNoRulesPolicy, &path));
  ASSERT_TRUE(policy2->InitFromFile(path));
  EXPECT_FALSE(policy2->IsAllowed(GURL("http://www.foo.com/blah.html"),
                                  FilePath("/usr/local/bin/test.so")));
  EXPECT_FALSE(policy2->IsAllowed(GURL("http://www.bar.com/blah.html"),
                                  FilePath("/usr/local/bin/test.so")));
  EXPECT_FALSE(policy2->IsAllowed(GURL("http://www.baz.com/blah.html"),
                                  FilePath("/usr/local/bin/test.so")));
  EXPECT_TRUE(policy2->IsAllowed(GURL("http://www.baz.com/blah.html"),
                                 FilePath("/usr/local/bin/real.so")));

  scoped_refptr<PluginSelectionPolicy> policy3 = new PluginSelectionPolicy;
  ASSERT_TRUE(CreatePolicy("empty", kEmptyPolicy, &path));
  ASSERT_TRUE(policy3->InitFromFile(path));
  EXPECT_TRUE(policy3->IsAllowed(GURL("http://www.foo.com/blah.html"),
                                 FilePath("/usr/local/bin/test.so")));
  EXPECT_TRUE(policy3->IsAllowed(GURL("http://www.bar.com/blah.html"),
                                 FilePath("/usr/local/bin/test.so")));
  EXPECT_TRUE(policy3->IsAllowed(GURL("http://www.baz.com/blah.html"),
                                 FilePath("/usr/local/bin/test.so")));
  EXPECT_TRUE(policy3->IsAllowed(GURL("http://www.baz.com/blah.html"),
                                 FilePath("/usr/local/bin/real.so")));
}

TEST_F(PluginSelectionPolicyTest, MissingFile) {
  // Don't create any policy file, just get the path to one.
  FilePath path = GetPolicyPath("missing_file");
  scoped_refptr<PluginSelectionPolicy> policy = new PluginSelectionPolicy;
  ASSERT_FALSE(policy->InitFromFile(path));
  EXPECT_TRUE(policy->IsAllowed(GURL("http://www.foo.com/blah.html"),
                                FilePath("/usr/local/bin/allow_foo.so")));
  EXPECT_TRUE(policy->IsAllowed(GURL("http://www.bar.com/blah.html"),
                                FilePath("/usr/local/bin/allow_foo.so")));
}

TEST_F(PluginSelectionPolicyTest, FindFirstAllowed) {
  FilePath path;
  ASSERT_TRUE(CreatePolicy("multi", kMultiPluginTestPolicy, &path));
  scoped_refptr<PluginSelectionPolicy> policy = new PluginSelectionPolicy;
  ASSERT_TRUE(policy->InitFromFile(path));
  EXPECT_TRUE(policy->IsAllowed(GURL("http://www.foo.com/blah.html"),
                                FilePath("/usr/local/bin/allow_foo.so")));
  EXPECT_FALSE(policy->IsAllowed(GURL("http://www.bar.com/blah.html"),
                                 FilePath("/usr/local/bin/allow_foo.so")));
  EXPECT_FALSE(policy->IsAllowed(GURL("http://www.baz.com/blah.html"),
                                 FilePath("/usr/local/bin/allow_foo.so")));
  EXPECT_FALSE(policy->IsAllowed(GURL("http://www.bim.com/blah.html"),
                                 FilePath("/usr/local/bin/allow_foo.so")));
  EXPECT_FALSE(policy->IsAllowed(GURL("http://www.foo.com/blah.html"),
                                 FilePath("/usr/local/bin/allow_baz_bim1.so")));
  EXPECT_FALSE(policy->IsAllowed(GURL("http://www.bar.com/blah.html"),
                                 FilePath("/usr/local/bin/allow_baz_bim1.so")));
  EXPECT_TRUE(policy->IsAllowed(GURL("http://www.baz.com/blah.html"),
                                FilePath("/usr/local/bin/allow_baz_bim1.so")));
  EXPECT_TRUE(policy->IsAllowed(GURL("http://www.bim.com/blah.html"),
                                FilePath("/usr/local/bin/allow_baz_bim1.so")));
  EXPECT_FALSE(policy->IsAllowed(GURL("http://www.google.com/blah.html"),
                                 FilePath("/usr/local/bin/allow_baz_bim1.so")));
  EXPECT_FALSE(policy->IsAllowed(GURL("http://www.foo.com/blah.html"),
                                 FilePath("/usr/local/bin/allow_baz_bim2.so")));
  EXPECT_FALSE(policy->IsAllowed(GURL("http://www.bar.com/blah.html"),
                                 FilePath("/usr/local/bin/allow_baz_bim2.so")));
  EXPECT_TRUE(policy->IsAllowed(GURL("http://www.baz.com/blah.html"),
                                FilePath("/usr/local/bin/allow_baz_bim2.so")));
  EXPECT_TRUE(policy->IsAllowed(GURL("http://www.bim.com/blah.html"),
                                FilePath("/usr/local/bin/allow_baz_bim2.so")));
  EXPECT_FALSE(policy->IsAllowed(GURL("http://www.google.com/blah.html"),
                                 FilePath("/usr/local/bin/allow_baz_bim2.so")));
  std::vector<webkit::npapi::WebPluginInfo> info_vector;
  webkit::npapi::WebPluginInfo info;
  // First we test that the one without any policy gets
  // selected for all if it's first.
  info.path = FilePath("/usr/local/bin/no_policy.so");
  info_vector.push_back(info);
  info.path = FilePath("/usr/local/bin/allow_foo.so");
  info_vector.push_back(info);
  info.path = FilePath("/usr/local/bin/allow_baz_bim1.so");
  info_vector.push_back(info);
  info.path = FilePath("/usr/local/bin/allow_baz_bim2.so");
  info_vector.push_back(info);
  EXPECT_EQ(0, policy->FindFirstAllowed(GURL("http://www.baz.com/blah.html"),
                                        info_vector));
  EXPECT_EQ(0, policy->FindFirstAllowed(GURL("http://www.foo.com/blah.html"),
                                        info_vector));
  EXPECT_EQ(0, policy->FindFirstAllowed(GURL("http://www.bling.com/blah.html"),
                                        info_vector));
  EXPECT_EQ(0, policy->FindFirstAllowed(GURL("http://www.google.com/blah.html"),
                                        info_vector));

  // Now move the plugin without any policy to the end.
  info_vector.clear();
  info.path = FilePath("/usr/local/bin/allow_foo.so");
  info_vector.push_back(info);
  info.path = FilePath("/usr/local/bin/allow_baz_bim1.so");
  info_vector.push_back(info);
  info.path = FilePath("/usr/local/bin/allow_baz_bim2.so");
  info_vector.push_back(info);
  info.path = FilePath("/usr/local/bin/no_policy.so");
  info_vector.push_back(info);
  EXPECT_EQ(1, policy->FindFirstAllowed(GURL("http://www.baz.com/blah.html"),
                                        info_vector));
  EXPECT_EQ(0, policy->FindFirstAllowed(GURL("http://www.foo.com/blah.html"),
                                        info_vector));
  EXPECT_EQ(3, policy->FindFirstAllowed(GURL("http://www.bling.com/blah.html"),
                                        info_vector));
  EXPECT_EQ(3, policy->FindFirstAllowed(GURL("http://www.google.com/blah.html"),
                                        info_vector));
}

}  // namespace chromeos
