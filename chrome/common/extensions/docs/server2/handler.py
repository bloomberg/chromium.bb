# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from appengine_wrappers import taskqueue
from commit_tracker import CommitTracker
from cron_servlet import CronServlet
from instance_servlet import InstanceServlet
from object_store_creator import ObjectStoreCreator
from patch_servlet import PatchServlet
from refresh_servlet import RefreshServlet
from servlet import Servlet, Request, Response
from test_servlet import TestServlet


_DEFAULT_SERVLET = InstanceServlet.GetConstructor()


class _EnqueueServlet(Servlet):
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


class _QueryCommitServlet(Servlet):
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
    return Response.Ok(commit_tracker.Get(self._request.path).Get())


_SERVLETS = {
  'cron': CronServlet,
  'enqueue': _EnqueueServlet,
  'patch': PatchServlet,
  'query_commit': _QueryCommitServlet,
  'refresh': RefreshServlet,
  'test': TestServlet,
}


class Handler(Servlet):
  def Get(self):
    path = self._request.path

    if path.startswith('_'):
      servlet_path = path[1:]
      if not '/' in servlet_path:
        servlet_path += '/'
      servlet_name, servlet_path = servlet_path.split('/', 1)
      servlet = _SERVLETS.get(servlet_name)
      if servlet is None:
        return Response.NotFound('"%s" servlet not found' %  servlet_path)
    else:
      servlet_path = path
      servlet = _DEFAULT_SERVLET

    return servlet(Request(servlet_path,
                           self._request.host,
                           self._request.headers,
                           self._request.arguments)).Get()
