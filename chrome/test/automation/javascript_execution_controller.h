// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_JAVASCRIPT_EXECUTION_CONTROLLER_H_
#define CHROME_TEST_AUTOMATION_JAVASCRIPT_EXECUTION_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class JavaScriptExecutionController;

// This class is a proxy to an object in JavaScript. It holds a handle which
// can be used to retrieve the actual object in JavaScript scripts.
class JavaScriptObjectProxy
    : public base::RefCountedThreadSafe<JavaScriptObjectProxy> {
 public:
  JavaScriptObjectProxy(JavaScriptExecutionController* executor, int handle);
  virtual ~JavaScriptObjectProxy();

  // Returns JavaScript which can be used for retrieving the actual object
  // associated with this proxy.
  std::string GetReferenceJavaScript();

  int handle() const { return handle_; }
  bool is_valid() const { return executor_; }

 protected:
  base::WeakPtr<JavaScriptExecutionController> executor_;
  int handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(JavaScriptObjectProxy);
};

// This class handles the execution of arbitrary JavaScript, preparing it for
// execution, and parsing its result (in JSON). It keeps track of all returned
// JavaScript objects.
class JavaScriptExecutionController
    : public base::SupportsWeakPtr<JavaScriptExecutionController> {
 public:
  JavaScriptExecutionController() {}
  virtual ~JavaScriptExecutionController() {}

  // Executes |script| and parse return value.
  // A corresponding ConvertResponse(Value* value, T* result) must exist
  // for type T.
  template <typename T>
  bool ExecuteJavaScriptAndParse(const std::string& script, T* result) {
    std::string json;
    if (!ExecuteJavaScript(script, &json))
      return false;
    scoped_ptr<Value> value;
    if (!ParseJSON(json, &value))
      return false;
    return ConvertResponse(value.get(), result);
  }

  // Executes |script| with no return.
  bool ExecuteJavaScript(const std::string& script);

  // Returns JavaScript which can be used for retrieving the actual object
  // associated with the proxy |object|.
  static std::string GetReferenceJavaScript(JavaScriptObjectProxy* object);

  // Returns the equivalent JSON for |vector|.
  static std::string Serialize(const std::vector<std::string>& vector);

 protected:
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
  // Called by JavaScriptObjectProxy on destruct.
  void Remove(int handle);

  bool ParseJSON(const std::string& json, scoped_ptr<Value>* result);

  bool ExecuteJavaScript(const std::string& script, std::string* json);

  bool ConvertResponse(Value* value, bool* result);
  bool ConvertResponse(Value* value, int* result);
  bool ConvertResponse(Value* value, std::string* result);

  template<class JavaScriptObject>
  bool ConvertResponse(Value* value, JavaScriptObject** result) {
    int handle;
    if (!value->GetAsInteger(&handle))
      return false;

    HandleToObjectMap::const_iterator iter = handle_to_object_.find(handle);
    if (iter == handle_to_object_.end()) {
      *result = new JavaScriptObject(this, handle);
      if (handle_to_object_.empty())
        FirstObjectAdded();
      handle_to_object_.insert(std::make_pair(handle, *result));
    } else {
      *result = static_cast<JavaScriptObject*>(iter->second);
    }
    return true;
  }

  template<typename T>
  bool ConvertResponse(Value* value, std::vector<T>* result) {
    if (!value->IsType(Value::TYPE_LIST))
      return false;

    ListValue* list = static_cast<ListValue*>(value);
    for (size_t i = 0; i < list->GetSize(); i++) {
      Value* inner_value;
      if (!list->Get(i, &inner_value))
        return false;
      T item;
      ConvertResponse(inner_value, &item);
      result->push_back(item);
    }
    return true;
  }

  // Weak pointer to all the object proxies that we create.
  HandleToObjectMap handle_to_object_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptExecutionController);
};

#endif  // CHROME_TEST_AUTOMATION_JAVASCRIPT_EXECUTION_CONTROLLER_H_
