// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/command_executor_impl.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/fake_session_accessor.h"
#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webdriver/atoms.h"

namespace {

class StubChrome : public Chrome {
 public:
  StubChrome() {}
  virtual ~StubChrome() {}

  // Overridden from Chrome:
  virtual Status Load(const std::string& url) OVERRIDE {
    return Status(kOk);
  }
  virtual Status EvaluateScript(const std::string& frame,
                                const std::string& function,
                                scoped_ptr<base::Value>* result) OVERRIDE {
    return Status(kOk);
  }
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::ListValue& args,
                              scoped_ptr<base::Value>* result) OVERRIDE {
    return Status(kOk);
  }
  virtual Status GetFrameByFunction(const std::string& frame,
                                    const std::string& function,
                                    const base::ListValue& args,
                                    std::string* out_frame) OVERRIDE {
    return Status(kOk);
  }
  virtual Status Quit() OVERRIDE {
    return Status(kOk);
  }
};

class OkLauncher : public ChromeLauncher {
 public:
  OkLauncher() {}
  virtual ~OkLauncher() {}

  // Overridden from ChromeLauncher:
  virtual Status Launch(const FilePath& chrome_exe,
                        scoped_ptr<Chrome>* chrome) OVERRIDE {
    chrome->reset(new StubChrome());
    return Status(kOk);
  }
};

}  // namespace

TEST(CommandsTest, NewSession) {
  OkLauncher launcher;
  SessionMap map;
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string session_id;
  Status status =
      ExecuteNewSession(&map, &launcher, params, "", &value, &session_id);
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(value);
  base::DictionaryValue* dict;
  ASSERT_TRUE(value->GetAsDictionary(&dict));
  std::string browserName;
  ASSERT_TRUE(dict->GetString("browserName", &browserName));
  ASSERT_STREQ("chrome", browserName.c_str());

  scoped_refptr<SessionAccessor> accessor;
  ASSERT_TRUE(map.Get(session_id, &accessor));
  scoped_ptr<base::AutoLock> lock;
  Session* session = accessor->Access(&lock);
  ASSERT_TRUE(session);
  ASSERT_STREQ(session_id.c_str(), session->id.c_str());
  ASSERT_TRUE(session->chrome);
}

namespace {

class FailLauncher : public ChromeLauncher {
 public:
  FailLauncher() {}
  virtual ~FailLauncher() {}

  // Overridden from ChromeLauncher:
  virtual Status Launch(const FilePath& chrome_exe,
                        scoped_ptr<Chrome>* chrome) OVERRIDE {
    return Status(kUnknownError);
  }
};

}  // namespace

TEST(CommandsTest, NewSessionLauncherFails) {
  FailLauncher launcher;
  SessionMap map;
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string session_id;
  Status status =
      ExecuteNewSession(&map, &launcher, params, "", &value, &session_id);
  ASSERT_EQ(kSessionNotCreatedException, status.code());
  ASSERT_FALSE(value);
}

namespace {

Status ExecuteStubQuit(
    int* count,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* value,
    std::string* out_session_id) {
  if (*count == 0) {
    EXPECT_STREQ("id", session_id.c_str());
  } else {
    EXPECT_STREQ("id2", session_id.c_str());
  }
  (*count)++;
  return Status(kOk);
}

}  // namespace

TEST(CommandsTest, QuitAll) {
  SessionMap map;
  Session session("id");
  Session session2("id2");
  map.Set(session.id,
          scoped_refptr<SessionAccessor>(new FakeSessionAccessor(&session)));
  map.Set(session2.id,
          scoped_refptr<SessionAccessor>(new FakeSessionAccessor(&session2)));

  int count = 0;
  Command cmd = base::Bind(&ExecuteStubQuit, &count);
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string session_id;
  Status status =
      ExecuteQuitAll(cmd, &map, params, "", &value, &session_id);
  ASSERT_EQ(kOk, status.code());
  ASSERT_FALSE(value.get());
  ASSERT_EQ(2, count);
}

TEST(CommandsTest, Quit) {
  SessionMap map;
  Session session("id", scoped_ptr<Chrome>(new StubChrome()));
  map.Set(session.id,
          scoped_refptr<SessionAccessor>(new FakeSessionAccessor(&session)));
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  ASSERT_EQ(kOk, ExecuteQuit(&map, &session, params, &value).code());
  ASSERT_FALSE(map.Has(session.id));
  ASSERT_FALSE(value.get());
}

namespace {

class FailsToQuitChrome : public StubChrome {
 public:
  FailsToQuitChrome() {}
  virtual ~FailsToQuitChrome() {}

  // Overridden from Chrome:
  virtual Status Quit() OVERRIDE {
    return Status(kUnknownError);
  }
};

}  // namespace

TEST(CommandsTest, QuitFails) {
  SessionMap map;
  Session session("id", scoped_ptr<Chrome>(new FailsToQuitChrome()));
  map.Set(session.id,
          scoped_refptr<SessionAccessor>(new FakeSessionAccessor(&session)));
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  ASSERT_EQ(kUnknownError, ExecuteQuit(&map, &session, params, &value).code());
  ASSERT_FALSE(map.Has(session.id));
  ASSERT_FALSE(value.get());
}

namespace {

class FindElementChrome : public StubChrome {
 public:
  explicit FindElementChrome(bool element_exists)
      : element_exists_(element_exists), called_(false) {}
  virtual ~FindElementChrome() {}

  const std::string& GetFrame() { return frame_; }
  const std::string& GetFunction() { return function_; }
  const base::ListValue* GetArgs() { return args_.get(); }

  // Overridden from Chrome:
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::ListValue& args,
                              scoped_ptr<base::Value>* result) OVERRIDE {
    if (element_exists_ && called_) {
      base::DictionaryValue element;
      element.SetString("ELEMENT", "1");
      result->reset(element.DeepCopy());
      frame_ = frame;
      function_ = function;
      args_.reset(args.DeepCopy());
    } else {
      result->reset(base::Value::CreateNullValue());
    }
    called_ = true;
    return Status(kOk);
  }
 private:
  bool element_exists_;
  bool called_;
  std::string frame_;
  std::string function_;
  scoped_ptr<base::ListValue> args_;
};

}  // namespace

TEST(CommandsTest, SuccessfulFindElement) {
  FindElementChrome* chrome = new FindElementChrome(true);
  Session session("id", scoped_ptr<Chrome>(chrome));
  session.implicit_wait = 100;
  session.frame = "frame_id1";
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  scoped_ptr<base::Value> value;
  ASSERT_EQ(kOk, ExecuteFindElement(&session, params, &value).code());
  base::DictionaryValue* element;
  ASSERT_TRUE(value->GetAsDictionary(&element));
  ASSERT_EQ(1U, element->size());
  std::string element_id;
  ASSERT_TRUE(element->GetString("ELEMENT", &element_id));
  ASSERT_EQ("1", element_id);
  ASSERT_EQ("frame_id1", chrome->GetFrame());
  ASSERT_EQ(webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENT),
            chrome->GetFunction());
  const base::ListValue* args = chrome->GetArgs();
  ASSERT_TRUE(args);
  ASSERT_EQ(1U, args->GetSize());
  const base::DictionaryValue* dict;
  ASSERT_TRUE(args->GetDictionary(0U, &dict));
  ASSERT_EQ(1U, dict->size());
  std::string id;
  ASSERT_TRUE(dict->GetString("id", &id));
  ASSERT_EQ("a", id);
}

TEST(CommandsTest, FailedFindElement) {
  Session session("id", scoped_ptr<Chrome>(new FindElementChrome(false)));
  session.implicit_wait = 0;
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  scoped_ptr<base::Value> value;
  ASSERT_EQ(kNoSuchElement,
            ExecuteFindElement(&session, params, &value).code());
}

namespace {

class FindElementsChrome : public StubChrome {
 public:
  explicit FindElementsChrome(bool element_exists)
      : element_exists_(element_exists), called_(false) {}
  virtual ~FindElementsChrome() {}

  const std::string& GetFrame() { return frame_; }
  const std::string& GetFunction() { return function_; }
  const base::ListValue* GetArgs() { return args_.get(); }

  // Overridden from Chrome:
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::ListValue& args,
                              scoped_ptr<base::Value>* result) OVERRIDE {
    if (element_exists_ && called_) {
      base::DictionaryValue element1;
      element1.SetString("ELEMENT", "1");
      base::DictionaryValue element2;
      element2.SetString("ELEMENT", "2");
      base::ListValue list;
      list.Append(element1.DeepCopy());
      list.Append(element2.DeepCopy());
      result->reset(list.DeepCopy());
      frame_ = frame;
      function_ = function;
      args_.reset(args.DeepCopy());
    } else {
      result->reset(new base::ListValue());
    }
    called_ = true;
    return Status(kOk);
  }
 private:
  bool element_exists_;
  bool called_;
  std::string frame_;
  std::string function_;
  scoped_ptr<base::ListValue> args_;
};

}  //namespace

TEST(CommandsTest, SuccessfulFindElements) {
  FindElementsChrome* chrome = new FindElementsChrome(true);
  Session session("id", scoped_ptr<Chrome>(chrome));
  session.implicit_wait = 100;
  session.frame = "frame_id2";
  base::DictionaryValue params;
  params.SetString("using", "name");
  params.SetString("value", "b");
  scoped_ptr<base::Value> value;
  ASSERT_EQ(kOk, ExecuteFindElements(&session, params, &value).code());
  base::ListValue* list;
  ASSERT_TRUE(value->GetAsList(&list));
  ASSERT_EQ(2U, list->GetSize());
  base::DictionaryValue* element1;
  ASSERT_TRUE(list->GetDictionary(0U, &element1));
  ASSERT_EQ(1U, element1->size());
  std::string element1_id;
  ASSERT_TRUE(element1->GetString("ELEMENT", &element1_id));
  ASSERT_EQ("1", element1_id);
  base::DictionaryValue* element2;
  ASSERT_TRUE(list->GetDictionary(1U, &element2));
  ASSERT_EQ(1U, element2->size());
  std::string element2_id;
  ASSERT_TRUE(element2->GetString("ELEMENT", &element2_id));
  ASSERT_EQ("2", element2_id);
  ASSERT_EQ("frame_id2", chrome->GetFrame());
  ASSERT_EQ(webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENTS),
            chrome->GetFunction());
  const base::ListValue* args = chrome->GetArgs();
  ASSERT_TRUE(args);
  ASSERT_EQ(1U, args->GetSize());
  const base::DictionaryValue* dict;
  ASSERT_TRUE(args->GetDictionary(0U, &dict));
  ASSERT_EQ(1U, dict->size());
  std::string name;
  ASSERT_TRUE(dict->GetString("name", &name));
  ASSERT_EQ("b", name);
}

TEST(CommandsTest, FailedFindElements) {
  Session session("id", scoped_ptr<Chrome>(new FindElementsChrome(false)));
  session.implicit_wait = 0;
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  scoped_ptr<base::Value> value;
  ASSERT_EQ(kOk, ExecuteFindElements(&session, params, &value).code());
  base::ListValue* list;
  ASSERT_TRUE(value->GetAsList(&list));
  ASSERT_EQ(0U, list->GetSize());
}

namespace {

class ErrorCallFunctionChrome : public StubChrome {
 public:
  ErrorCallFunctionChrome() {}
  virtual ~ErrorCallFunctionChrome() {}

  // Overridden from Chrome:
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::ListValue& args,
                              scoped_ptr<base::Value>* result) OVERRIDE {
    return Status(kUnknownError);
  }
};

}  // namespace

TEST(CommandsTest, ErrorFindElement) {
  Session session("id", scoped_ptr<Chrome>(new ErrorCallFunctionChrome()));
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  scoped_ptr<base::Value> value;
  ASSERT_EQ(kUnknownError, ExecuteFindElement(&session, params, &value).code());
  ASSERT_EQ(kUnknownError,
            ExecuteFindElements(&session, params, &value).code());
}
