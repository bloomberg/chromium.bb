# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A service for dispatching and managing work based on load.

This module provides the `WorkQueueService`, which implements a simple
RPC service.  Calls to the service are executed in strict FIFO order.
Workload is metered, so that the amount of work currently underway stays
below a target threshold.  When workload goes above the threshold, new
calls block on the server side until capacity is available.
"""

from __future__ import print_function

import errno
import os
import pickle
import shutil
import time

from chromite.lib import cros_logging as logging


class WorkQueueTimeout(Exception):
  """Exception to raise when `WorkQueueService.Wait()` times out."""
  def __init__(self, request_id, timeout):
    super(WorkQueueTimeout, self).__init__(
        'Request "{:s}" timed out after {:d} seconds'.format(
            request_id, timeout))


class _BaseWorkQueue(object):
  """Work queue code common to both server- and client-side.

  Requests in a work queue are tracked using a spool directory.  New
  requests are added to the spool directory, as are final results.
  In the usual flow, requests work their way through these 4 states:
    _REQUESTED - Clients add new requests into the spool in this state
      from `EnqueueRequest()`.
    _PENDING - The server moves each request to this state from
      "requested" when it sees the request for the first time in
      `ProcessRequests()`.
    _RUNNING - The server moves the request to this state when it
      schedules a task for the request in `ProcessRequests()`.
    _COMPLETE - The server moves a request to this state when its running
      task returns a result.  The waiting client then retrieves the
      result and removes the request from the spool.

  Each state has a corresponding, distinct subdirectory in the spool.
  Individual requests are recorded in files in the subdirectory
  corresponding to the request's current state.

  Requests are identified by a timestamp assigned when they are
  enqueued.  Requests are handled in strict FIFO order, based on that
  timestamp.

  A request can be aborted if the client chooses to give up waiting
  for the result.  To do this, the client creates an abort request in
  the `_ABORTING` directory using the identifier of the request to be
  aborted.
    * An aborted request that is not yet complete will never report a
      result.
    * An aborted request that is already complete will be removed, and
      its result will then be unavailable.
    * Attempts to abort requests that no longer exist will be ignored.

  Consistency within the spool is guaranteed by a simple ownership
  protocol:
    * Only clients are allowed to create files in the `_REQUESTED` or
      `_ABORTING` subdirectories.  The client implementation guarantees
      that request ids are globally unique within the spool.
    * Only the server can move request files from one state to the next.
      The server is single threaded to guarante that transitions are
      atomic.
    * Normally, only the client can remove files from `_COMPLETE`.  The
      client transfers responsibility for the final removal by making
      an abort request.

  Clients are not meant to abort requests they did not create, but the
  API provides no enforcement of this requirement.  System behavior in
  this event is still consistent:
    * If the request is not valid, the server will ignore it.
    * If the request is valid, a client still waiting on the result
      will eventually time out.
  """

  # The various states of requests.
  #
  # For convenience, the names of the state directories are selected so
  # that when sorted lexically, they appear in the order in which they're
  # processed.

  _REQUESTED = '1-requested'
  _PENDING = '2-pending'
  _RUNNING = '3-running'
  _COMPLETE = '4-complete'
  _ABORTING = '5-aborting'
  _STATES = [_REQUESTED, _PENDING, _RUNNING, _COMPLETE, _ABORTING]

  # _REQUEST_ID_FORMAT
  # In the client's EnqueueRequest() method, requests are given an id
  # based on the time when the request arrives.  This format string is
  # used to create the id from the timestamp.
  #
  # Format requirements:
  #   * When sorted lexically, id values must reflect the order of
  #     their arrival.
  #   * The id values must be usable as file names.
  #   * For performance, the id should include enough digits from the
  #     timestamp such that 1) collisions with other clients is
  #     unlikely, and 2) in a retry loop, collisions with the prior
  #     iteration are unlikely.
  #
  # This value is given a name partly for the benefit of the unit tests,
  # which patch an alternative value that artifically increases the
  # chance retrying due to collision.

  _REQUEST_ID_FORMAT = '{:.6f}'

  def __init__(self, spool_dir):
    self._spool_state_dirs = {state: os.path.join(spool_dir, state)
                              for state in self._STATES}

  def _GetRequestPathname(self, request_id, state):
    """Return the path to a request's data given its state."""
    request_dir = self._spool_state_dirs[state]
    return os.path.join(request_dir, request_id)

  def _RequestInState(self, request_id, state):
    """Return whether `request_id` is in state `state`."""
    return os.path.exists(self._GetRequestPathname(request_id, state))

  def _MakeRequestId(self):
    return self._REQUEST_ID_FORMAT.format(time.time())


class WorkQueueClient(_BaseWorkQueue):
  """A service for dispatching tasks based on system load.

  Typical usage:

      workqueue = service.WorkQueueClient(SPOOL_DIR)
      request_id = workqueue.EnqueueRequest(request_arguments)
      result = workqueue.Wait(request_id, timeout_value)

  Explanation of the usage:
    * `SPOOL_DIR` is the path to the work queue's spool directory.
    * `request_arguments` represents an object acceptable as an
      argument to the call provided by the server.
    * `timeout_value` is a time in seconds representing how long the
      client is willing to wait for the result.

  Requests that time out or for whatever reason no longer have a client
  waiting for them can be aborted by calling `AbortRequest`.  Aborted
  requests are removed from the spool directory, and will never return
  a result.
  """

  # _WAIT_POLL_INTERVAL
  # Time in seconds to wait between polling checks in the `Wait()`
  # method.
  _WAIT_POLL_INTERVAL = 1.0

  def _IsComplete(self, request_id):
    """Test whether `request_id` is in "completed" state."""
    return self._RequestInState(request_id, self._COMPLETE)

  def EnqueueRequest(self, request):
    """Create a new request for processing.

    Args:
      request: An object encapsulating the work to be done.

    Returns:
      the `request_id` identifying the work for calls to `Wait()` or
      `AbortRequest()`.
    """
    fd = -1
    while True:
      request_id = self._MakeRequestId()
      request_path = self._GetRequestPathname(request_id, self._REQUESTED)
      try:
        # os.O_EXCL guarantees that the file did not exist until this
        # call created it.  So, if some other client creates the request
        # file before us, we'll fail with EEXIST.
        fd = os.open(request_path, os.O_EXCL | os.O_CREAT | os.O_WRONLY)
        break
      except OSError as oserr:
        if oserr.errno != errno.EEXIST:
          raise
    with os.fdopen(fd, 'w') as f:
      pickle.dump(request, f)
    return request_id

  def AbortRequest(self, request_id):
    """Abort the given `request_id`.

    Aborted requests are removed from the spool directory.  If running,
    their results will be dropped and never returned.

    The intended usage of this method is that this will be called (only)
    by the specific client that created the request to be aborted.
    Clients still waiting for a request when it is aborted will
    eventually time out.

    Args:
      request_id: The id of the request to be aborted.
    """
    request_path = self._GetRequestPathname(request_id, self._ABORTING)
    open(request_path, 'w').close()

  def Wait(self, request_id, timeout):
    """Wait for completion of a given request.

    If a result is made available for the request within the timeout,
    that result will be returned.  If the server side encountered an
    exception, the exception will be re-raised.  If wait time for the
    request exceeds `timeout` seconds, the request will be aborted, and
    a `WorkQueueTimeout` exception will be raised.

    Args:
      request_id: Id of the request to wait for.
      timeout: How long to wait before timing out.

    Returns:
      the result object reported for the task.

    Raises:
      WorkQueueTimeout:   raised when the wait time exceeds the given
                          timeout value.
      Exception:          any subclass of `Exception` may be raised if
                          returned by the server.
    """
    end_time = time.time() + timeout
    while not self._IsComplete(request_id):
      if end_time < time.time():
        self.AbortRequest(request_id)
        raise WorkQueueTimeout(request_id, timeout)
      time.sleep(self._WAIT_POLL_INTERVAL)
    completion_file = self._GetRequestPathname(request_id, self._COMPLETE)
    with open(completion_file, 'r') as f:
      result = pickle.load(f)
    os.remove(completion_file)
    if isinstance(result, Exception):
      raise result
    assert not isinstance(result, BaseException)
    return result


class WorkQueueServer(_BaseWorkQueue):
  """A service for dispatching tasks based on system load.

  Typical usage:

      workqueue = service.WorkQueueService(SPOOL_DIR)
      workqueue.ProcessRequests(task_manager)

  Explanation of the usage:
    * `SPOOL_DIR` is the path to the work queue's spool directory.
    * `task_manager` is an instance of a concrete subclass of
      `tasks.TaskManager`.

  The server code in this class is independent of the details of the
  tasks being scheduled; the `ProcessRequests()` delegates management of
  tasks in the `_RUNNING` state to its `task_manager` parameter.  The
  task manager object is responsible for these actions:
    * Starting new tasks.
    * Reporting results from completed tasks.
    * Aborting running tasks when requested.
    * Indicating whether capacity is available to start new tasks.
  """

  # _HEARTBEAT_INTERVAL -
  # `ProcessRequests()` periodically logs a message at the start of
  # tick, just so you can see it's alive.  This value determines the
  # approximate time in seconds in between messages.

  _HEARTBEAT_INTERVAL = 10 * 60

  def _CreateSpool(self):
    """Create and populate the spool directory in the file system."""
    spool_dir = os.path.dirname(self._spool_state_dirs[self._REQUESTED])
    if os.path.exists(spool_dir):
      for old_path in os.listdir(spool_dir):
        shutil.rmtree(os.path.join(spool_dir, old_path))
    else:
      os.mkdir(spool_dir)
    for state_dir in self._spool_state_dirs.itervalues():
      os.mkdir(state_dir)

  def _TransitionRequest(self, request_id, oldstate, newstate):
    """Move a request from one state to another."""
    logging.info('Transition %s from %s to %s',
                 request_id, oldstate, newstate)
    oldpath = self._GetRequestPathname(request_id, oldstate)
    newpath = self._GetRequestPathname(request_id, newstate)
    os.rename(oldpath, newpath)

  def _ClearRequest(self, request_id, state):
    """Remove a request given its state."""
    os.remove(self._GetRequestPathname(request_id, state))

  def _CompleteRequest(self, request_id, result):
    """Move a task that has finished running into "completed" state."""
    logging.info('Reaped %s, result = %r', request_id, result)
    completion_path = self._GetRequestPathname(request_id, self._COMPLETE)
    with open(completion_path, 'w') as f:
      pickle.dump(result, f)
    self._ClearRequest(request_id, self._RUNNING)

  def _GetRequestsByState(self, state):
    """Return all requests in a given state."""
    requests_dir = self._spool_state_dirs[state]
    return sorted(os.listdir(requests_dir))

  def _GetNewRequests(self):
    """Move all tasks in `requested` state to `pending` state."""
    new_requests = self._GetRequestsByState(self._REQUESTED)
    if new_requests:
      while self._MakeRequestId() == new_requests[-1]:
        pass
    for request_id in new_requests:
      self._TransitionRequest(request_id, self._REQUESTED, self._PENDING)
    return new_requests

  def _GetAbortRequests(self):
    """Move all tasks in `requested` state to `pending` state."""
    new_requests = self._GetRequestsByState(self._ABORTING)
    for request_id in new_requests:
      logging.info('Abort requested for %s', request_id)
      self._ClearRequest(request_id, self._ABORTING)
    return new_requests

  def _StartRequest(self, request_id, manager):
    """Start execution of a given request."""
    pending_path = self._GetRequestPathname(request_id, self._PENDING)
    with open(pending_path, 'r') as f:
      request_object = pickle.load(f)
    manager.StartTask(request_id, request_object)
    self._TransitionRequest(request_id, self._PENDING, self._RUNNING)

  def _ProcessAbort(self, request_id, pending_requests, manager):
    """Actually remove a given request that is being aborted."""
    state = None
    if self._RequestInState(request_id, self._PENDING):
      pending_requests.remove(request_id)
      state = self._PENDING
    elif self._RequestInState(request_id, self._RUNNING):
      manager.TerminateTask(request_id)
      state = self._RUNNING
    elif self._RequestInState(request_id, self._COMPLETE):
      state = self._COMPLETE
    # No check for "requested" state; our caller guarantees it's not
    # needed.
    #
    # By design, we don't fail if the aborted request is already gone.
    if state is not None:
      logging.info('Abort is removing %s from state %s',
                   request_id, state)
      try:
        self._ClearRequest(request_id, state)
      except OSError:
        logging.exception('Request %s was not removed from %s.',
                          request_id, state)
    else:
      logging.info('Abort for non-existent request %s', request_id)

  def ProcessRequests(self, manager):
    """Main processing loop for the server-side daemon.

    The method runs indefinitely; it terminates only if an exception is
    raised during its execution.

    Args:
      manager: An instance of `tasks.TaskManager`.  This object is
                responsible for starting and tracking running tasks.
    """
    self._CreateSpool()
    pending_requests = []
    tick_count = 0
    next_heartbeat = time.time()
    num_running = 0
    while True:
      tick_count += 1
      if next_heartbeat <= time.time():
        next_heartbeat = time.time() + self._HEARTBEAT_INTERVAL
        logging.debug('Starting tick number %d', tick_count)
      manager.StartTick()
      num_completed = 0
      for request_id, result in manager.Reap():
        num_completed += 1
        self._CompleteRequest(request_id, result)
      num_running -= num_completed
      num_pending = len(pending_requests)
      pending_requests.extend(self._GetNewRequests())
      num_added = len(pending_requests) - num_pending
      num_aborted = 0
      for abort_id in self._GetAbortRequests():
        num_aborted += 1
        self._ProcessAbort(abort_id, pending_requests, manager)
      num_started = 0
      while pending_requests and manager.HasCapacity():
        num_started += 1
        self._StartRequest(pending_requests.pop(0), manager)
      num_running += num_started
      if num_completed or num_added or num_aborted or num_started:
        logging.info('new: %d, started: %d, aborted: %d, completed: %d',
                     num_added, num_started, num_aborted, num_completed)
        logging.info('pending: %d, running %d',
                     len(pending_requests), num_running)
      time.sleep(manager.sample_interval)
