// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/keyword_extensions_delegate_impl.h"

#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/omnibox/keyword_provider.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

class ScopedExtensionLoadObserver : public ExtensionRegistryObserver {
 public:
  ScopedExtensionLoadObserver(ExtensionRegistry* registry,
                              const base::Closure& quit_closure);
  virtual ~ScopedExtensionLoadObserver();

 private:
  virtual void OnExtensionInstalled(content::BrowserContext* browser_context,
                                    const Extension* extension,
                                    bool is_update) OVERRIDE;

  ExtensionRegistry* registry_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExtensionLoadObserver);
};

ScopedExtensionLoadObserver::ScopedExtensionLoadObserver(
    ExtensionRegistry* registry,
    const base::Closure& quit_closure)
    : registry_(registry),
      quit_closure_(quit_closure) {
  registry_->AddObserver(this);
}

ScopedExtensionLoadObserver::~ScopedExtensionLoadObserver() {
  registry_->RemoveObserver(this);
}

void ScopedExtensionLoadObserver::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  quit_closure_.Run();
}

class KeywordExtensionsDelegateImplTest : public ExtensionServiceTestBase {
 public:
  KeywordExtensionsDelegateImplTest() {}
  virtual ~KeywordExtensionsDelegateImplTest() {}

 protected:
  virtual void SetUp() OVERRIDE;

  void RunTest(bool incognito);

 private:
  DISALLOW_COPY_AND_ASSIGN(KeywordExtensionsDelegateImplTest);
};

void KeywordExtensionsDelegateImplTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeExtensionService(CreateDefaultInitParams());
  InitializeProcessManager();
}

void KeywordExtensionsDelegateImplTest::RunTest(bool incognito) {
  TemplateURLService empty_model(NULL, 0);
  scoped_refptr<KeywordProvider> keyword_provider =
      new KeywordProvider(NULL, &empty_model);

  // Load an extension.
  {
    base::FilePath path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
    path = path.AppendASCII("extensions").AppendASCII("good_unpacked");

    base::RunLoop run_loop;
    ScopedExtensionLoadObserver load_observer(registry(),
                                              run_loop.QuitClosure());

    scoped_refptr<UnpackedInstaller> installer(
        UnpackedInstaller::Create(service()));
    installer->Load(path);

    run_loop.Run();
  }

  ASSERT_EQ(1U, registry()->enabled_extensions().size());
  scoped_refptr<const Extension> extension =
      *(registry()->enabled_extensions().begin());
  ASSERT_FALSE(util::IsIncognitoEnabled(extension->id(), profile()));

  Profile* profile_to_use = incognito ?
      profile()->GetOffTheRecordProfile() : profile();
  KeywordExtensionsDelegateImpl delegate_impl(profile_to_use,
                                              keyword_provider.get());
  KeywordExtensionsDelegate* delegate = &delegate_impl;
  EXPECT_NE(incognito, delegate->IsEnabledExtension(extension->id()));

  // Enable the extension in incognito mode, which requires a reload.
  {
    base::RunLoop run_loop;
    ScopedExtensionLoadObserver load_observer(registry(),
                                              run_loop.QuitClosure());

    util::SetIsIncognitoEnabled(extension->id(), profile(), true);

    run_loop.Run();
  }

  ASSERT_EQ(1U, registry()->enabled_extensions().size());
  extension = *(registry()->enabled_extensions().begin());
  ASSERT_TRUE(util::IsIncognitoEnabled(extension->id(), profile()));
  EXPECT_TRUE(delegate->IsEnabledExtension(extension->id()));
}

TEST_F(KeywordExtensionsDelegateImplTest, IsEnabledExtension) {
  RunTest(false);
}

TEST_F(KeywordExtensionsDelegateImplTest, IsEnabledExtensionIncognito) {
  RunTest(true);
}

}  // namespace

}  // namespace extensions
