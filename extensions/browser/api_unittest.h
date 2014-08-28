// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_UNITTEST_H_
#define EXTENSIONS_BROWSER_API_UNITTEST_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/mock_extension_system.h"

namespace base {
class Value;
class DictionaryValue;
class ListValue;
}

namespace content {
class NotificationService;
class TestBrowserThreadBundle;
}

class UIThreadExtensionFunction;

namespace extensions {

// Use this class to enable calling API functions in a unittest.
// By default, this class will create and load an empty unpacked |extension_|,
// which will be used in all API function calls. This extension can be
// overridden using set_extension().
// When calling RunFunction[AndReturn*], |args| should be in JSON format,
// wrapped in a list. See also RunFunction* in api_test_utils.h.
class ApiUnitTest : public ExtensionsTest {
 public:
  ApiUnitTest();
  virtual ~ApiUnitTest();

  const Extension* extension() const { return extension_.get(); }
  scoped_refptr<Extension> extension_ref() { return extension_; }
  void set_extension(scoped_refptr<Extension> extension) {
    extension_ = extension;
  }

 protected:
  // SetUp creates and loads an empty, unpacked Extension.
  virtual void SetUp() OVERRIDE;

  // Various ways of running an API function. These methods take ownership of
  // |function|. |args| should be in JSON format, wrapped in a list.
  // See also the RunFunction* methods in extension_function_test_utils.h.

  // Return the function result as a base::Value.
  scoped_ptr<base::Value> RunFunctionAndReturnValue(
      UIThreadExtensionFunction* function,
      const std::string& args);

  // Return the function result as a base::DictionaryValue, or NULL.
  // This will EXPECT-fail if the result is not a DictionaryValue.
  scoped_ptr<base::DictionaryValue> RunFunctionAndReturnDictionary(
      UIThreadExtensionFunction* function,
      const std::string& args);

  // Return the function result as a base::ListValue, or NULL.
  // This will EXPECT-fail if the result is not a ListValue.
  scoped_ptr<base::ListValue> RunFunctionAndReturnList(
      UIThreadExtensionFunction* function,
      const std::string& args);

  // Return an error thrown from the function, if one exists.
  // This will EXPECT-fail if any result is returned from the function.
  std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                        const std::string& args);

  // Run the function and ignore any result.
  void RunFunction(UIThreadExtensionFunction* function,
                   const std::string& args);

 private:
  scoped_ptr<content::NotificationService> notification_service_;

  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  TestingPrefServiceSimple testing_pref_service_;

  MockExtensionSystemFactory<MockExtensionSystem> extension_system_factory_;

  // The Extension used when running API function calls.
  scoped_refptr<Extension> extension_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_UNITTEST_H_
