# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class WebElement(object):
  """Represents an HTML element."""
  def __init__(self, chromedriver, id_):
    self._chromedriver = chromedriver
    self._id = id_

  def _Execute(self, command, params=None):
    if params is None:
      params = {}
    params['id'] = self._id;
    return self._chromedriver.ExecuteSessionCommand(command, params)

  def FindElement(self, strategy, target):
    return self._Execute(
        'findChildElement', {'using': strategy, 'value': target})

  def FindElements(self, strategy, target):
    return self._Execute(
        'findChildElements', {'using': strategy, 'value': target})

  def HoverOver(self):
    self._Execute('hoverOverElement')

  def Click(self):
    self._Execute('clickElement')

  def Clear(self):
    self._Execute('clearElement')
