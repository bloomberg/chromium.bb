// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session_commands.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/session.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SessionCommandsTest, ExecuteGetTimeouts) {
  Session session("id");
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;

  Status status = ExecuteGetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());
  base::DictionaryValue* response;
  ASSERT_TRUE(value->GetAsDictionary(&response));

  int script;
  ASSERT_TRUE(response->GetInteger("script", &script));
  ASSERT_EQ(script, 30000);
  int page_load;
  ASSERT_TRUE(response->GetInteger("pageLoad", &page_load));
  ASSERT_EQ(page_load, 300000);
  int implicit;
  ASSERT_TRUE(response->GetInteger("implicit", &implicit));
  ASSERT_EQ(implicit, 0);
}

TEST(SessionCommandsTest, MergeCapabilities) {
  base::DictionaryValue primary;
  primary.SetString("strawberry", "velociraptor");
  primary.SetString("pear", "unicorn");

  base::DictionaryValue secondary;
  secondary.SetString("broccoli", "giraffe");
  secondary.SetString("celery", "hippo");
  secondary.SetString("eggplant", "elephant");

  base::DictionaryValue merged;

  // key collision should return false
  ASSERT_FALSE(MergeCapabilities(&primary, &primary, &merged));
  // non key collision should return true
  ASSERT_TRUE(MergeCapabilities(&primary, &secondary, &merged));

  merged.Clear();
  MergeCapabilities(&primary, &secondary, &merged);
  primary.MergeDictionary(&secondary);

  ASSERT_EQ(primary, merged);
}

TEST(SessionCommandsTest, FileUpload) {
  Session session("id");
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;
  // Zip file entry that contains a single file with contents 'COW\n', base64
  // encoded following RFC 1521.
  const char kBase64ZipEntry[] =
      "UEsDBBQAAAAAAMROi0K/wAzGBAAAAAQAAAADAAAAbW9vQ09XClBLAQIUAxQAAAAAAMROi0K/"
      "wAzG\nBAAAAAQAAAADAAAAAAAAAAAAAACggQAAAABtb29QSwUGAAAAAAEAAQAxAAAAJQAAAA"
      "AA\n";
  params.SetString("file", kBase64ZipEntry);
  Status status = ExecuteUploadFile(&session, params, &value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::FilePath::StringType path;
  ASSERT_TRUE(value->GetAsString(&path));
  ASSERT_TRUE(base::PathExists(base::FilePath(path)));
  std::string data;
  ASSERT_TRUE(base::ReadFileToString(base::FilePath(path), &data));
  ASSERT_STREQ("COW\n", data.c_str());
}

namespace {

class DetachChrome : public StubChrome {
 public:
  DetachChrome() : quit_called_(false) {}
  ~DetachChrome() override {}

  // Overridden from Chrome:
  Status Quit() override {
    quit_called_ = true;
    return Status(kOk);
  }

  bool quit_called_;
};

}  // namespace

TEST(SessionCommandsTest, MatchCapabilities) {
  base::DictionaryValue merged;
  merged.SetString("browserName", "not chrome");

  ASSERT_FALSE(MatchCapabilities(&merged));

  merged.Clear();
  merged.SetString("browserName", "chrome");

  ASSERT_TRUE(MatchCapabilities(&merged));
}

TEST(SessionCommandsTest, Quit) {
  DetachChrome* chrome = new DetachChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));

  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;

  ASSERT_EQ(kOk, ExecuteQuit(false, &session, params, &value).code());
  ASSERT_TRUE(chrome->quit_called_);

  chrome->quit_called_ = false;
  ASSERT_EQ(kOk, ExecuteQuit(true, &session, params, &value).code());
  ASSERT_TRUE(chrome->quit_called_);
}

TEST(SessionCommandsTest, QuitWithDetach) {
  DetachChrome* chrome = new DetachChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  session.detach = true;

  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;

  ASSERT_EQ(kOk, ExecuteQuit(true, &session, params, &value).code());
  ASSERT_FALSE(chrome->quit_called_);

  ASSERT_EQ(kOk, ExecuteQuit(false, &session, params, &value).code());
  ASSERT_TRUE(chrome->quit_called_);
}

namespace {

class FailsToQuitChrome : public StubChrome {
 public:
  FailsToQuitChrome() {}
  ~FailsToQuitChrome() override {}

  // Overridden from Chrome:
  Status Quit() override { return Status(kUnknownError); }
};

}  // namespace

TEST(SessionCommandsTest, QuitFails) {
  Session session("id", std::unique_ptr<Chrome>(new FailsToQuitChrome()));
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;
  ASSERT_EQ(kUnknownError, ExecuteQuit(false, &session, params, &value).code());
}

TEST(SessionCommandsTest, AutoReporting) {
  DetachChrome* chrome = new DetachChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;
  StatusCode status_code;
  bool enabled;

  // autoreporting should be disabled by default
  status_code = ExecuteIsAutoReporting(&session, params, &value).code();
  ASSERT_EQ(kOk, status_code);
  ASSERT_FALSE(session.auto_reporting_enabled);
  ASSERT_TRUE(value.get()->GetAsBoolean(&enabled));
  ASSERT_FALSE(enabled);

  // an error should be given if the |enabled| parameter is not set
  status_code = ExecuteSetAutoReporting(&session, params, &value).code();
  ASSERT_EQ(kUnknownError, status_code);

  // try to enable autoreporting
  params.SetBoolean("enabled", true);
  status_code = ExecuteSetAutoReporting(&session, params, &value).code();
  ASSERT_EQ(kOk, status_code);
  ASSERT_TRUE(session.auto_reporting_enabled);

  // check that autoreporting was enabled successfully
  status_code = ExecuteIsAutoReporting(&session, params, &value).code();
  ASSERT_EQ(kOk, status_code);
  ASSERT_TRUE(value.get()->GetAsBoolean(&enabled));
  ASSERT_TRUE(enabled);

  // try to disable autoreporting
  params.SetBoolean("enabled", false);
  status_code = ExecuteSetAutoReporting(&session, params, &value).code();
  ASSERT_EQ(kOk, status_code);
  ASSERT_FALSE(session.auto_reporting_enabled);

  // check that autoreporting was disabled successfully
  status_code = ExecuteIsAutoReporting(&session, params, &value).code();
  ASSERT_EQ(kOk, status_code);
  ASSERT_TRUE(value.get()->GetAsBoolean(&enabled));
  ASSERT_FALSE(enabled);
}
