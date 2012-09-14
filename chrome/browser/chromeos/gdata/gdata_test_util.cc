// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_test_util.h"

#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace gdata {
namespace test_util {

// This class is used to monitor if any task is posted to a message loop.
class TaskObserver : public MessageLoop::TaskObserver {
 public:
  TaskObserver() : posted_(false) {}
  virtual ~TaskObserver() {}

  // MessageLoop::TaskObserver overrides.
  virtual void WillProcessTask(base::TimeTicks time_posted) {}
  virtual void DidProcessTask(base::TimeTicks time_posted) {
    posted_ = true;
  }

  // Returns true if any task was posted.
  bool posted() const { return posted_; }

 private:
  bool posted_;
  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

FilePath GetTestFilePath(const std::string& relative_path) {
  FilePath path;
  std::string error;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("chromeos")
      .Append(FilePath::FromUTF8Unsafe(relative_path));
  EXPECT_TRUE(file_util::PathExists(path)) <<
      "Couldn't find " << path.value();
  return path;
}

void RunBlockingPoolTask() {
  while (true) {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();

    TaskObserver task_observer;
    MessageLoop::current()->AddTaskObserver(&task_observer);
    MessageLoop::current()->RunAllPending();
    MessageLoop::current()->RemoveTaskObserver(&task_observer);
    if (!task_observer.posted())
      break;
  }
}


scoped_ptr<base::Value> LoadJSONFile(const std::string& relative_path) {
  FilePath path = GetTestFilePath(relative_path);

  std::string error;
  JSONFileValueSerializer serializer(path);
  scoped_ptr<base::Value> value(serializer.Deserialize(NULL, &error));
  EXPECT_TRUE(value.get()) <<
      "Parse error " << path.value() << ": " << error;
  return value.Pass();
}

}  // namespace test_util
}  // namespace gdata
