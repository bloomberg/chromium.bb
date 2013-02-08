// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares the C++ side of PyAuto, the python interface to
// Chromium automation.  It access Chromium's internals using Automation Proxy.

#ifndef CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_
#define CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui/ui_test_suite.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

class AutomationProxy;

// The C++ style guide forbids using default arguments but I'm taking the
// liberty of allowing it in this file. The sole purpose of this (and the
// .cc) is to support the python interface, and default args are allowed in
// python. Strictly adhering to the guide here would mean having to re-define
// all methods in python just for the sake of providing default args. This
// seems cumbersome and unwanted.

// Test Suite for Pyauto tests. All one-time initializations go here.
class PyUITestSuiteBase : public UITestSuite {
 public:
  PyUITestSuiteBase(int argc, char** argv);
  virtual ~PyUITestSuiteBase();

  void InitializeWithPath(const base::FilePath& browser_dir);

  void SetCrSourceRoot(const base::FilePath& path);

 private:
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool_;
#endif
};

// The primary class that interfaces with Automation Proxy.
// This class is accessed from python using swig.
class PyUITestBase : public UITestBase {
 public:
  // Only public methods are accessible from swig.

  // Constructor. Lookup pyauto.py for doc on these args.
  PyUITestBase(bool clear_profile, std::wstring homepage);
  virtual ~PyUITestBase();

  // Initialize the setup. Should be called before launching the browser.
  // |browser_dir| is the path to dir containing chromium binaries.
  void Initialize(const base::FilePath& browser_dir);

  void UseNamedChannelID(const std::string& named_channel_id) {
    named_channel_id_ = named_channel_id;
    launcher_.reset(CreateProxyLauncher());
  }

  virtual ProxyLauncher* CreateProxyLauncher() OVERRIDE;

  // SetUp,TearDown is redeclared as public to make it accessible from swig.
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // AutomationProxy methods

  // Meta-methods.  Generic pattern of passing args and response as
  // JSON dict to avoid future use of the SWIG interface and
  // automation proxy additions.  Returns response as JSON dict.
  // Use -ve window_index for automation calls not targetted at a browser
  // window.  Example: Login call for chromeos.
  std::string _SendJSONRequest(int window_index,
                               const std::string& request,
                               int timeout);

  // Sets a cookie value for a url. Returns true on success.
  bool SetCookie(const GURL& cookie_url, const std::string& value,
                 int window_index = 0, int tab_index = 0);
  // Gets a cookie value for the given url.
  std::string GetCookie(const GURL& cookie_url, int window_index = 0,
                        int tab_index = 0);

 protected:
  // Gets the automation proxy and checks that it exists.
  virtual AutomationProxy* automation() const OVERRIDE;

  virtual void SetLaunchSwitches() OVERRIDE;

 private:
  // Create JSON error responses.
  void ErrorResponse(const std::string& error_string,
                     const std::string& request,
                     bool is_timeout,
                     std::string* response);
  void RequestFailureResponse(
      const std::string& request,
      const base::TimeDelta& duration,
      const base::TimeDelta& timeout,
      std::string* response);

  // Enables PostTask to main thread.
  // Should be shared across multiple instances of PyUITestBase so that this
  // class is re-entrant and multiple instances can be created.
  // This is necessary since python's unittest module creates instances of
  // TestCase at load time itself.
  static MessageLoop* GetSharedMessageLoop(MessageLoop::Type msg_loop_type);
  static MessageLoop* message_loop_;

  // Path to named channel id.
  std::string named_channel_id_;
};

#endif  // CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_
