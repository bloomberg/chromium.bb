// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/WebTask.h"

#include <algorithm>
#include "third_party/WebKit/public/web/WebKit.h"

using namespace std;

namespace WebTestRunner {

WebTask::WebTask(WebTaskList* list)
    : m_taskList(list)
{
    m_taskList->registerTask(this);
}

WebTask::~WebTask()
{
    if (m_taskList)
        m_taskList->unregisterTask(this);
}

WebTaskList::WebTaskList()
{
}

WebTaskList::~WebTaskList()
{
    revokeAll();
}

void WebTaskList::registerTask(WebTask* task)
{
    m_tasks.push_back(task);
}

void WebTaskList::unregisterTask(WebTask* task)
{
    vector<WebTask*>::iterator iter = find(m_tasks.begin(), m_tasks.end(), task);
    if (iter != m_tasks.end())
        m_tasks.erase(iter);
}

void WebTaskList::revokeAll()
{
    while (!m_tasks.empty())
        m_tasks[0]->cancel();
}

}
