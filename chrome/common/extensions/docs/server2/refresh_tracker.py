# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from future import All, Future


class _RefreshWorkOrder(object):
  '''A set of tasks that must be completed in a refresh cycle.'''
  def __init__(self, tasks):
    self.tasks = set(tasks)


class _RefreshTaskCompletion(object):
  '''Marks the completion of a single named task for some refresh cycle.'''
  def __init__(self, task):
    self.task = task


def _TaskKey(refresh_id, task):
  return '%s@%s' % (refresh_id, task)


class RefreshTracker(object):
  '''Manages and tracks the progress of data refresh cycles.'''
  def __init__(self, object_store_creator):
    # These object stores should never be created empty since they are strictly
    # used to persist state across requests.
    self._work_orders = object_store_creator.Create(_RefreshWorkOrder,
        start_empty=False)
    self._task_completions = object_store_creator.Create(_RefreshTaskCompletion,
        start_empty=False)

  def _GetWorkOrder(self, refresh_id):
    '''Retrieves the work order for a refresh cycle identified by |id|.'''
    return self._work_orders.Get(refresh_id)

  def StartRefresh(self, refresh_id, tasks):
    work_order = _RefreshWorkOrder(tasks)
    return self._work_orders.Set(refresh_id, work_order)

  def GetRefreshComplete(self, refresh_id):
    '''Determines if a refresh cycle identified by |id| is complete. Returns
    a |Future| which resolves to either |True| or |False|.'''
    def is_work_order_complete(work_order):
      # If the identified refresh cycle is not known, always return False.
      if work_order is None:
        return False
      return (self._task_completions.GetMulti(
                _TaskKey(refresh_id, task) for task in work_order.tasks)
              .Then(lambda tasks: len(tasks) == len(work_order.tasks)))
    return self._GetWorkOrder(refresh_id).Then(is_work_order_complete)

  def MarkTaskComplete(self, refresh_id, task):
    return self._task_completions.Set(_TaskKey(refresh_id, task),
        _RefreshTaskCompletion(task))
