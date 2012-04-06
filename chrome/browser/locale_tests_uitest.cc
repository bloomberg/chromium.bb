// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/environment.h"
#include "build/build_config.h"
#include "ui/base/ui_base_switches.h"

class LocaleTestsBase : public UITest {
 public:
  LocaleTestsBase() : UITest(), old_lc_all_(NULL) {
  }

  virtual void TearDown() {
#if defined(OS_LINUX)
    scoped_ptr<base::Environment> env(base::Environment::Create());
    if (old_lc_all_) {
      env->SetVar("LC_ALL", old_lc_all_);
    } else {
      env->UnSetVar("LC_ALL");
    }
#endif
    UITest::TearDown();
  }

 protected:
  const char* old_lc_all_;
};


class LocaleTestsDa : public LocaleTestsBase {
 public:
  LocaleTestsDa() : LocaleTestsBase() {
    launch_arguments_.AppendSwitchASCII(switches::kLang, "da");

    // Linux doesn't use --lang, it only uses environment variables to set the
    // language.
#if defined(OS_LINUX)
    old_lc_all_ = getenv("LC_ALL");
    setenv("LC_ALL", "da_DK.UTF-8", 1);
#endif
  }
};

class LocaleTestsHe : public LocaleTestsBase {
 public:
  LocaleTestsHe() : LocaleTestsBase() {
    launch_arguments_.AppendSwitchASCII(switches::kLang, "he");
#if defined(OS_LINUX)
    old_lc_all_ = getenv("LC_ALL");
    setenv("LC_ALL", "he_IL.UTF-8", 1);
#endif
  }
};

class LocaleTestsZhTw : public LocaleTestsBase {
 public:
  LocaleTestsZhTw() : LocaleTestsBase() {
    launch_arguments_.AppendSwitchASCII(switches::kLang, "zh-TW");
#if defined(OS_LINUX)
    old_lc_all_ = getenv("LC_ALL");
    setenv("LC_ALL", "zh_TW.UTF-8", 1);
#endif
  }
};

#if defined(OS_CHROMEOS)
// See http://crbug.com/122366
#define MAYBE_TestStart FAILS_TestStart
#else
#define MAYBE_TestStart TestStart
#endif
TEST_F(LocaleTestsDa, MAYBE_TestStart) {
  // Just making sure we can start/shutdown cleanly.
}

TEST_F(LocaleTestsHe, TestStart) {
  // Just making sure we can start/shutdown cleanly.
}

TEST_F(LocaleTestsZhTw, TestStart) {
  // Just making sure we can start/shutdown cleanly.
}
