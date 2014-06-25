# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Manages intents and associated information.

This is generally intended to be used with functions that calls Android's
Am command.
"""

class Intent(object):

  def __init__(self, action='android.intent.action.VIEW', activity=None,
               category=None, component=None, data=None, extras=None,
               flags=None, package=None):
    """Creates an Intent.

    Args:
      action: A string containing the action.
      activity: A string that, with |package|, can be used to specify the
                component.
      category: A string or list containing any categories.
      component: A string that specifies the component to send the intent to.
      data: A string containing a data URI.
      extras: A dict containing extra parameters to be passed along with the
              intent.
      flags: A string containing flags to pass.
      package: A string that, with activity, can be used to specify the
               component.
    """
    self._action = action
    self._activity = activity
    if isinstance(category, list) or category is None:
      self._category = category
    else:
      self._category = [category]
    self._component = component
    self._data = data
    self._extras = extras
    self._flags = flags
    self._package = package

    if self._component and '/' in component:
      self._package, self._activity = component.split('/', 1)
    elif self._package and self._activity:
      self._component = '%s/%s' % (package, activity)

  @property
  def action(self):
    return self._action

  @property
  def activity(self):
    return self._activity

  @property
  def category(self):
    return self._category

  @property
  def component(self):
    return self._component

  @property
  def data(self):
    return self._data

  @property
  def extras(self):
    return self._extras

  @property
  def flags(self):
    return self._flags

  @property
  def package(self):
    return self._package

