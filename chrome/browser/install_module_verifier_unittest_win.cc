// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_module_verifier_win.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/enumerate_modules_model_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void AddModule(base::ListValue* list,
               ModuleEnumerator::ModuleType type,
               const std::string& name) {
  scoped_ptr<base::DictionaryValue> data(new DictionaryValue());
  data->SetInteger("type", type);
  data->SetString("name", name);
  list->Append(data.release());
}

}  // namespace

TEST(InstallModuleVerifierTest, ExtractLoadedModuleNameDigests) {
  std::set<std::string> loaded_module_name_digests;
  scoped_ptr<base::ListValue> list(new ListValue());
  ExtractLoadedModuleNameDigests(*list, &loaded_module_name_digests);
  ASSERT_TRUE(loaded_module_name_digests.empty());
  // WinSock modules are ignored.
  AddModule(list.get(),
            ModuleEnumerator::SHELL_EXTENSION, "winsock_module.dll");
  // Shell Extension modules are ignored.
  AddModule(
      list.get(),
      ModuleEnumerator::WINSOCK_MODULE_REGISTRATION, "shell_extension.dll");
  AddModule(list.get(), ModuleEnumerator::LOADED_MODULE, "fancy_pants.dll");
  AddModule(list.get(), ModuleEnumerator::LOADED_MODULE, "chrome.dll");
  ExtractLoadedModuleNameDigests(*list, &loaded_module_name_digests);
  ASSERT_EQ(2, loaded_module_name_digests.size());
  ASSERT_NE(loaded_module_name_digests.end(),
            loaded_module_name_digests.find(base::MD5String("chrome.dll")));
  ASSERT_NE(loaded_module_name_digests.end(),
            loaded_module_name_digests.find(
                base::MD5String("fancy_pants.dll")));
}

TEST(InstallModuleVerifierTest, VerifyModules) {
  std::set<std::string> loaded_module_name_digests;
  std::set<size_t> reported_module_ids;

  VerifyModules(loaded_module_name_digests,
                AdditionalModules(),
                &reported_module_ids);
  ASSERT_TRUE(reported_module_ids.empty());

  // Expected, Loaded modules are reported.
  loaded_module_name_digests.insert(base::MD5String("chrome.dll"));
  // Unexpected modules are ignored.
  loaded_module_name_digests.insert(base::MD5String("fancy_pants.dll"));
  VerifyModules(loaded_module_name_digests,
                AdditionalModules(),
                &reported_module_ids);
  ASSERT_EQ(1, reported_module_ids.size());
  // chrome.dll
  ASSERT_NE(reported_module_ids.end(), reported_module_ids.find(1u));
  reported_module_ids.clear();

  // AdditionalModules can be used to detect other modules.
  AdditionalModules additional_modules;
  std::string fancy_pants_md5_digest(base::MD5String("fancy_pants.dll"));
  additional_modules.push_back(
      std::make_pair(123u, base::StringPiece(fancy_pants_md5_digest)));
  VerifyModules(loaded_module_name_digests,
                additional_modules,
                &reported_module_ids);
  ASSERT_EQ(2, reported_module_ids.size());
  // chrome.dll
  ASSERT_NE(reported_module_ids.end(), reported_module_ids.find(1u));
  // fancy_pants.dll
  ASSERT_NE(reported_module_ids.end(), reported_module_ids.find(123u));
  reported_module_ids.clear();

  // TODO(erikwright): Verify case-insensitive.
}

TEST(InstallModuleVerifierTest, ParseAdditionalModules) {
  std::vector<std::pair<size_t, base::StringPiece> > additional_modules;

  ParseAdditionalModules(base::StringPiece(), &additional_modules);
  ASSERT_EQ(0u, additional_modules.size());

  // Invalid input.
  ParseAdditionalModules("hello", &additional_modules);
  ASSERT_EQ(0u, additional_modules.size());
  ParseAdditionalModules("hello world", &additional_modules);
  ASSERT_EQ(0u, additional_modules.size());
  ParseAdditionalModules("hello world\nfoo bar", &additional_modules);
  ASSERT_EQ(0u, additional_modules.size());
  ParseAdditionalModules("123 world\nfoo bar", &additional_modules);
  ASSERT_EQ(0u, additional_modules.size());

  // A single valid line followed by an invalid line.
  ParseAdditionalModules("hello 0123456789abcdef0123456789abcdef\nfoo bar",
                         &additional_modules);
  ASSERT_EQ(0u, additional_modules.size());
  ParseAdditionalModules("123 0123456789abcdef0123456789abcdef\nfoo bar",
                         &additional_modules);
  ASSERT_EQ(1u, additional_modules.size());
  ASSERT_EQ(123u, additional_modules[0].first);
  ASSERT_EQ("0123456789abcdef0123456789abcdef", additional_modules[0].second);
  additional_modules.clear();

  // The same, but with \r\n.
  ParseAdditionalModules("123 0123456789abcdef0123456789abcdef\r\nfoo bar",
                         &additional_modules);
  ASSERT_EQ(1u, additional_modules.size());
  ASSERT_EQ(123u, additional_modules[0].first);
  ASSERT_EQ("0123456789abcdef0123456789abcdef", additional_modules[0].second);
  additional_modules.clear();

  // Several valid and invalid lines, with varying line terminations etc.
  ParseAdditionalModules("123 0123456789abcdef0123456789abcdef\r\n"
                         "456 DEADBEEFDEADBEEF\r"
                         "789 DEADBEEFDEADBEEFDEADBEEFDEADBEEF\n"
                         "\n"
                         "\n"
                         "321 BAADCAFEBAADCAFEBAADCAFEBAADCAFE\n",
                         &additional_modules);
  ASSERT_EQ(3u, additional_modules.size());
  ASSERT_EQ(123u, additional_modules[0].first);
  ASSERT_EQ("0123456789abcdef0123456789abcdef", additional_modules[0].second);
  ASSERT_EQ(789u, additional_modules[1].first);
  ASSERT_EQ("DEADBEEFDEADBEEFDEADBEEFDEADBEEF", additional_modules[1].second);
  ASSERT_EQ(321u, additional_modules[2].first);
  ASSERT_EQ("BAADCAFEBAADCAFEBAADCAFEBAADCAFE", additional_modules[2].second);
  additional_modules.clear();

  // Leading empty lines, no termination on final line.
  ParseAdditionalModules("\n"
                         "123 0123456789abcdef0123456789abcdef\r\n"
                         "321 BAADCAFEBAADCAFEBAADCAFEBAADCAFE",
                         &additional_modules);
  ASSERT_EQ(2u, additional_modules.size());
  ASSERT_EQ(123u, additional_modules[0].first);
  ASSERT_EQ("0123456789abcdef0123456789abcdef", additional_modules[0].second);
  ASSERT_EQ(321u, additional_modules[1].first);
  ASSERT_EQ("BAADCAFEBAADCAFEBAADCAFEBAADCAFE", additional_modules[1].second);
  additional_modules.clear();
}
