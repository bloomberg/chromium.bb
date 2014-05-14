// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTASK_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTASK_H_

#include <vector>

namespace content {

class WebTaskList;

// WebTask represents a task which can run by WebTestDelegate::postTask() or
// WebTestDelegate::postDelayedTask().
class WebTask {
public:
    explicit WebTask(WebTaskList*);
    virtual ~WebTask();

    // The main code of this task.
    // An implementation of run() should return immediately if cancel() was called.
    virtual void run() = 0;
    virtual void cancel() = 0;

protected:
    WebTaskList* m_taskList;
};

class WebTaskList {
public:
    WebTaskList();
    ~WebTaskList();
    void registerTask(WebTask*);
    void unregisterTask(WebTask*);
    void revokeAll();

private:
    std::vector<WebTask*> m_tasks;
};

// A task containing an object pointer of class T. Derived classes should
// override runIfValid() which in turn can safely invoke methods on the
// m_object. The Class T must have "WebTaskList* mutable_task_list()".
template<class T>
class WebMethodTask : public WebTask {
public:
    explicit WebMethodTask(T* object)
        : WebTask(object->mutable_task_list())
        , m_object(object)
    {
    }

    virtual ~WebMethodTask() { }

    virtual void run()
    {
        if (m_object)
            runIfValid();
    }

    virtual void cancel()
    {
        m_object = 0;
        m_taskList->unregisterTask(this);
        m_taskList = 0;
    }

    virtual void runIfValid() = 0;

protected:
    T* m_object;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTASK_H_
