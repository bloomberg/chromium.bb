// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/public/test/test_browser_thread.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

class ExtensionService;
class TestingProfile;

namespace base {
class Value;
}

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

// This class provides a minimal environment in which to create
// extensions and tabs for extension-related unittests.
class TestExtensionEnvironment {
 public:
  TestExtensionEnvironment();
  ~TestExtensionEnvironment();

  TestingProfile* profile() const;

  // Returns an ExtensionService created (and owned) by the
  // TestExtensionSystem created by the TestingProfile.
  ExtensionService* GetExtensionService();

  // Creates an Extension and registers it with the ExtensionService.
  // The Extension has a default manifest of {name: "Extension",
  // version: "1.0", manifest_version: 2}, and values in
  // manifest_extra override these defaults.
  const Extension* MakeExtension(const base::Value& manifest_extra);

  // Returns a test web contents that has a tab id.
  scoped_ptr<content::WebContents> MakeTab() const;

 private:
  MessageLoopForUI loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread file_blocking_thread_;
  content::TestBrowserThread io_thread_;
  // We may need to add the rest of the browser threads here.  This is
  // likely to be indicated by memory leaks in which the object was
  // expected to be freed by a DeleteSoon() call.

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif
  scoped_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_
