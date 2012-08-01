# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os


class TimelineModel(object):
  """A proxy for about:tracing's TimelineModel class.

  Test authors should never need to know that this class is a proxy.
  """
  @staticmethod
  def _EscapeForQuotedJavascriptExecution(js):
      # Poor man's string escape.
      return js.replace('\'', '\\\'');

  def __init__(self, js_executor, shim_id):
    self._js_executor = js_executor
    self._shim_id = shim_id

  # Warning: The JSON serialization process removes cyclic references.
  # TODO(eatnumber): regenerate these cyclic references on deserialization.
  def _CallModelMethod(self, method_name, *args):
    result = self._js_executor(
        """window.timelineModelShims['%s'].invokeMethod('%s', '%s')""" % (
            self._shim_id,
            self._EscapeForQuotedJavascriptExecution(method_name),
            self._EscapeForQuotedJavascriptExecution(json.dumps(args))
        )
    )
    if result['success']:
      return result['data']
    # TODO(eatnumber): Make these exceptions more reader friendly.
    raise RuntimeError(result)

  def __del__(self):
    self._js_executor("""
        window.timelineModelShims['%s'] = undefined;
        window.domAutomationController.send('');
    """ % self._shim_id)

  def GetAllThreads(self):
    return self._CallModelMethod('getAllThreads')

  def GetAllCpus(self):
    return self._CallModelMethod('getAllCpus')

  def GetAllProcesses(self):
    return self._CallModelMethod('getAllProcesses')

  def GetAllCounters(self):
    return self._CallModelMethod('getAllCounters')

  def FindAllThreadsNamed(self, name):
    return self._CallModelMethod('findAllThreadsNamed', name);
