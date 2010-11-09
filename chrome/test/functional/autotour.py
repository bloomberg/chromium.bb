#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import random
import time


"""Autotour is a semi automatic exploratory framework for exploring actions
defined on an object. It uses decorators to mark methods as actions and an
explorer object to explore.
"""

def GodelAction(weight=1, requires=''):
  """Action Decorator

  This function is the key to exploration. In effect, it annotates the wrapping
  function with attributes such as is_action = True and sets some weights and
  the requires condition. The explorer can invoke functions based on these
  attributes.

  Args:
    weight: Weight for the action, default is set to 1. Can be any number >= 0
    requires: Precondition for an action to be executed. This usually points
              to a function which returns a boolean result.
  """
  def custom(target):
    """This is the decorator which sets the attributes for
    is_action, weight and requires.

    Args:
      target: Function to be decorated.

    Returns:
      The wrapped function with correct attributes set.
    """
    def wrapper(self, *args, **kwargs):
      target(self, *args, **kwargs)
    wrapper.is_action = True
    wrapper.weight = weight
    wrapper.requires = requires
    return wrapper
  return custom


class Godel(object):
  """Base class for all exploratory objects.

  All objects that wish to be explored must inherit this class.
  It provides an important method GetActions, which looks at all the functions
  and returns only those that have the is_action attribute set.
  """

  def Initialize(self, uniqueid):
    self._uniqueid = uniqueid

  def GetName(self):
    return type(self).__name__ + str(self._uniqueid)

  def GetActions(self):
    """Gets all the actions for this class."""
    return [method for method in dir(self)
            if hasattr(getattr(self, method), 'is_action')]

  def GetWeight(self, method):
    """Returns the weight of a given method.

    Args:
      method: Name of the Method whose Weight is queried
    """
    method_obj = getattr(self, method)
    return getattr(method_obj, 'weight', 1)

  def SetWeight(self, method, weight):
    """Sets the weight for a given method."""
    method_obj = getattr(self, method)
    method_obj.im_func.weight = weight


class Explorer(object):
  """Explorer class that controls the exploration of the object.

  This class has methods to add the exploration object and
  initiate exploration on them.
  """

  def __init__(self):
    self._seed = time.time()
    logging.info('#Seeded with %s' % self._seed)
    random.seed(self._seed)
    self._actionlimit = -1
    self._godels = []
    self._fh = logging.FileHandler(str(self._seed))
    self._log = logging.getLogger()
    self._log.addHandler(self._fh)
    self._log.setLevel(logging.DEBUG)
    self._uniqueid = 0

  def NextId(self):
    """Gets the NextId by incrementing a counter."""
    self._uniqueid = self._uniqueid + 1
    return self._uniqueid

  def Add(self, obj):
    """Adds an object which inherits from Godel to be explored.

    Args:
      obj: Object to be explored which usually inherits from the Godel class.
    """
    uniqueid = self.NextId()
    obj.Initialize(uniqueid)
    name = type(obj).__name__
    self._log.info('%s = %s()' % (name + str(uniqueid), name))
    self._godels.append(obj)

  def MeetsRequirement(self, godel, methodname):
    """Method that returns true if the method's precondition is satisfied.
    It does so by using the attribute "Requires" which is set by the decorator
    and invokes it which must return a boolean value
    Args:
      godel: Godel object on which the requirement needs to be tested.
      methodname: Method name which needs to be called to test.
    Returns:
      True if the methodname invoked returned True or methodname was empty,
      False otherwise
    """
    method = getattr(godel, methodname)
    requires = method.im_func.requires
    if callable(requires):
      return requires(godel)
    else:
      if len(requires) > 0:
        precondition = getattr(godel, requires)
        return precondition()
      else:
        return True

  def GetAvailableActions(self):
    """Returns a list of only those actions that satisfy their preconditions"""
    action_list = []
    for godel in self._godels:
      for action in godel.GetActions():
        if self.MeetsRequirement(godel, action):
          action_list.append([godel, action, godel.GetWeight(action)])

    return action_list

  def Choose(self, action_list):
    """Choosing function which allows to choose a method based on random
    but weighted scale. So if one method has twice the weight, it is twice as
    likely to be choosen than the other.

    Args:
      action_list: A list of Actions from which to choose.

    Returns:
      Chosen Action or None.
    """
    total = sum([action_info[2] for action_info in action_list])
    # Find a pivot value randomly from he total weight.
    index = random.randint(0, total)
    for action_info in action_list:
      # Decrease the total weight by the current action weight.
      total = total - action_info[2]
      # If total has fallen below the pivot, then we select the current action
      if total <= index:
        return action_info;
    return None

  def Execute(self, action_info):
    """Executes the action and logs to console the action taken.

    Args:
      action_info: Action Info for the action to execute.
        action_info[0] is the object on which the action is to be invoked.
        action_info[1] is the name of the method which is to be invoked.
        action_info[2] is the weight of the method.

    """
    action = getattr(action_info[0], action_info[1])
    self._log.info('%s.%s()' % (action_info[0].GetName(), action_info[1]))
    action()

  def Explore(self, function=None):
    """Sets the exploration in progress by repeatedly seeing if
    any actions are available and if so continues to call them. It times out
    after specified action limit.

    Args:
      function: A function which can be called to determine if the execution
                should continue. This function is invoked after each step and
                if it returns True, execution stops. This is useful in writing
                tests which explore until a particular condition is met.

    Returns:
      True, if given |function| returns True, OR if no more action could be
      chosen. False, otherwise.
    """
    count = 0
    while(True):
      if self._actionlimit > 0 and count > self._actionlimit:
          return False
      action_list = self.GetAvailableActions()
      action_info = self.Choose(action_list)
      if action_info is None:
        return function is None
      self.Execute(action_info)
      count = count + 1
      if function is not None and function():
        return True