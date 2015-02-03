# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import taskqueue
from commit_tracker import CommitTracker
from future import All
from object_store_creator import ObjectStoreCreator
from refresh_tracker import RefreshTracker
from servlet import Servlet, Response


class EnqueueServlet(Servlet):
  '''This Servlet can be used to manually enqueue tasks on the default
  taskqueue. Useful for when an admin wants to manually force a specific
  DataSource refresh, but the refresh operation takes longer than the 60 sec
  timeout of a non-taskqueue request. For example, you might query

  /_enqueue/_refresh/content_providers/cr-native-client?commit=123ff65468dcafff0

  which will enqueue a task (/_refresh/content_providers/cr-native-client) to
  refresh the NaCl documentation cache for commit 123ff65468dcafff0.

  Access to this servlet should always be restricted to administrative users.
  '''
  def __init__(self, request):
    Servlet.__init__(self, request)

  def Get(self):
    queue = taskqueue.Queue()
    queue.add(taskqueue.Task(url='/%s' % self._request.path,
                             params=self._request.arguments))
    return Response.Ok('Task enqueued.')


class QueryCommitServlet(Servlet):
  '''Provides read access to the commit ID cache within the server. For example:

  /_query_commit/master

  will return the commit ID stored under the commit key "master" within the
  commit cache. Currently "master" is the only named commit we cache, and it
  corresponds to the commit ID whose data currently populates the data cache
  used by live instances.
  '''
  def __init__(self, request):
    Servlet.__init__(self, request)

  def Get(self):
    object_store_creator = ObjectStoreCreator(start_empty=False)
    commit_tracker = CommitTracker(object_store_creator)

    def generate_response(result):
      commit_id, history = result
      history_log = ''.join('%s: %s<br>' % (entry.datetime, entry.commit_id)
                            for entry in reversed(history))
      response = 'Current commit: %s<br><br>Most recent commits:<br>%s' % (
          commit_id, history_log)
      return response

    commit_name = self._request.path
    id_future = commit_tracker.Get(commit_name)
    history_future = commit_tracker.GetHistory(commit_name)
    return Response.Ok(
        All((id_future, history_future)).Then(generate_response).Get())


class DumpRefreshServlet(Servlet):
  def __init__(self, request):
    Servlet.__init__(self, request)

  def Get(self):
    object_store_creator = ObjectStoreCreator(start_empty=False)
    refresh_tracker = RefreshTracker(object_store_creator)
    commit_id = self._request.path
    work_order = refresh_tracker._GetWorkOrder(commit_id).Get()
    task_names = ['%s@%s' % (commit_id, task) for task in work_order.tasks]
    completions = refresh_tracker._task_completions.GetMulti(task_names).Get()
    missing = []
    for task in task_names:
      if task not in completions:
        missing.append(task)
    response = 'Missing:<br>%s' % ''.join('%s<br>' % task for task in missing)
    return Response.Ok(response)

class ResetCommitServlet(Servlet):
  '''Writes a new commit ID to the commit cache. For example:

  /_reset_commit/master/123456

  will reset the 'master' commit ID to '123456'. The provided commit MUST be
  in the named commit's recent history or it will be ignored.
  '''

  class Delegate(object):
    def CreateCommitTracker(self):
      return CommitTracker(ObjectStoreCreator(start_empty=False))

  def __init__(self, request, delegate=Delegate()):
    Servlet.__init__(self, request)
    self._delegate = delegate

  def Get(self):
    commit_tracker = self._delegate.CreateCommitTracker()
    commit_name, commit_id = self._request.path.split('/', 1)
    history = commit_tracker.GetHistory(commit_name).Get()
    if not any(entry.commit_id == commit_id for entry in history):
      return Response.BadRequest('Commit %s not cached.' % commit_id)
    commit_tracker.Set(commit_name, commit_id).Get()
    return Response.Ok('Commit "%s" updated to %s' % (commit_name, commit_id))

