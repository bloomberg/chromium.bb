// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_JAVASCRIPT_EXECUTION_CONTROLLER_H_
#define CHROME_TEST_AUTOMATION_JAVASCRIPT_EXECUTION_CONTROLLER_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/test/automation/javascript_message_utils.h"

class JavaScriptObjectProxy;

// This class handles the execution of arbitrary JavaScript, preparing it for
// execution, and parsing its result (in JSON). It keeps track of all returned
// JavaScript objects.
class JavaScriptExecutionController
    : public base::SupportsWeakPtr<JavaScriptExecutionController> {
 public:
  JavaScriptExecutionController();

  // Executes |script| and parse the return value. Returns whether the
  // execution and parsing succeeded.
  template <typename T>
  bool ExecuteJavaScriptAndGetReturn(const std::string& script, T* result) {
    scoped_ptr<Value> returnValue;
    if (!ExecuteAndParseHelper(WrapJavaScript(script), &returnValue))
      return false;
    return ValueConversionTraits<T>::SetFromValue(returnValue.get(), result);
  }

  // Similar to above, except that it does not get the return value.
  bool ExecuteJavaScript(const std::string& script);

  // Executes |script|, waits for it to send a JSON response, and parses the
  // return value. This call itself blocks, but the JavaScript responds
  // asynchronously. Returns whether the execution and parsing succeeded.
  // Will return false on timeouts.
  template <typename T>
  bool ExecuteAsyncJavaScriptAndGetReturn(const std::string& script,
                                          T* result) {
    scoped_ptr<Value> returnValue;
    if (!ExecuteAndParseHelper(WrapAsyncJavaScript(script), &returnValue))
      return false;
    return ValueConversionTraits<T>::SetFromValue(returnValue.get(), result);
  }

  // Similar to above, except that it does not get the return value.
  bool ExecuteAsyncJavaScript(const std::string& script);

  // Returns the proxy associated with |handle|, creating one if necessary.
  // The proxy must be inherit JavaScriptObjectProxy.
  template<class JavaScriptObject>
  JavaScriptObject* GetObjectProxy(int handle) {
    JavaScriptObject* obj = NULL;
    HandleToObjectMap::const_iterator iter = handle_to_object_.find(handle);
    if (iter == handle_to_object_.end()) {
      obj = new JavaScriptObject(this, handle);
      if (handle_to_object_.empty())
        FirstObjectAdded();
      handle_to_object_.insert(std::make_pair(handle, obj));
    } else {
      obj = static_cast<JavaScriptObject*>(iter->second);
    }
    return obj;
  }

  // Sets a timeout to be used for all JavaScript methods in which a response
  // is returned asynchronously.
  static void set_timeout(int timeout_ms) { timeout_ms_ = timeout_ms; }

 protected:
  virtual ~JavaScriptExecutionController();

  // Executes |script| and sets the JSON response |json|. Returns true
  // on success.
  virtual bool ExecuteJavaScriptAndGetJSON(const std::string& script,
                                           std::string* json) = 0;

  // Called when this controller is tracking its first object. Used by
  // reference counted subclasses.
  virtual void FirstObjectAdded() {}

  // Called when this controller is no longer tracking any objects. Used by
  // reference counted subclasses.
  virtual void LastObjectRemoved() {}

 private:
  typedef std::map<int, JavaScriptObjectProxy*> HandleToObjectMap;

  friend class JavaScriptObjectProxy;
  // Called by JavaScriptObjectProxy on destruction.
  void Remove(int handle);

  // Helper method for executing JavaScript and parsing the JSON response.
  // If successful, returns true and sets |returnValue| as the script's return
  // value.
  bool ExecuteAndParseHelper(const std::string& script,
                             scoped_ptr<Value>* returnValue);

  // Returns |script| wrapped and prepared for proper JavaScript execution,
  // via the JavaScript function domAutomation.evaluateJavaScript.
  std::string WrapJavaScript(const std::string& script);

  // Returns |script| wrapped and prepared for proper JavaScript execution
  // via the JavaScript function domAutomation.evaluateAsyncJavaScript.
  std::string WrapAsyncJavaScript(const std::string& script);

  // Timeout to use for all asynchronous methods.
  static int timeout_ms_;

  // Weak pointer to all the object proxies that we create.
  HandleToObjectMap handle_to_object_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptExecutionController);
};

#endif  // CHROME_TEST_AUTOMATION_JAVASCRIPT_EXECUTION_CONTROLLER_H_
