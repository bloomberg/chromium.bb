# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

# pylint: disable=F0401
from mojo.bindings import promise


class PromiseTest(unittest.TestCase):

  def setUp(self):
    self.accumulated = []

  def _AddToAccumulated(self, res):
    self.accumulated.append(res)
    return res

  def testResolve(self):
    p = promise.Promise.Resolve(0)
    self.assertEquals(p.state, promise.Promise.STATE_FULLFILLED)
    p.Then(self._AddToAccumulated)
    self.assertEquals(self.accumulated, [0])

  def testResolveToPromise(self):
    p = promise.Promise.Resolve(0)
    self.assertEquals(p.state, promise.Promise.STATE_FULLFILLED)
    q = promise.Promise.Resolve(p)
    self.assertEquals(p.state, promise.Promise.STATE_FULLFILLED)
    q.Then(self._AddToAccumulated)
    self.assertEquals(self.accumulated, [0])

  def testReject(self):
    p = promise.Promise.Reject(0)
    self.assertEquals(p.state, promise.Promise.STATE_REJECTED)
    p.Then(onRejected=self._AddToAccumulated)
    self.assertEquals(self.accumulated, [0])

  def testGeneratorFunctionResolve(self):
    (p, resolve, _) = _GetPromiseAndFunctions()
    self.assertEquals(p.state, promise.Promise.STATE_PENDING)
    p.Then(self._AddToAccumulated)
    resolve(0)
    self.assertEquals(p.state, promise.Promise.STATE_FULLFILLED)
    self.assertEquals(self.accumulated, [0])

  def testGeneratorFunctionReject(self):
    (p, _, reject) = _GetPromiseAndFunctions()
    self.assertEquals(p.state, promise.Promise.STATE_PENDING)
    p.Then(None, self._AddToAccumulated)
    reject(0)
    self.assertEquals(p.state, promise.Promise.STATE_REJECTED)
    self.assertEquals(self.accumulated, [0])

  def testGeneratorFunctionResolveToPromise(self):
    (p1, resolve, _) = _GetPromiseAndFunctions()
    p2 = promise.Promise(lambda x, y: x(p1))
    self.assertEquals(p2.state, promise.Promise.STATE_PENDING)
    p2.Then(self._AddToAccumulated)
    resolve(promise.Promise.Resolve(0))
    self.assertEquals(self.accumulated, [0])

  def testComputation(self):
    (p, resolve, _) = _GetPromiseAndFunctions()
    p.Then(lambda x: x+1).Then(lambda x: x+2).Then(self._AddToAccumulated)
    self.assertEquals(self.accumulated, [])
    resolve(0)
    self.assertEquals(self.accumulated, [3])

  def testRecoverAfterException(self):
    (p, resolve, _) = _GetPromiseAndFunctions()
    p.Then(_ThrowException).Catch(self._AddToAccumulated)
    self.assertEquals(self.accumulated, [])
    resolve(0)
    self.assertEquals(len(self.accumulated), 1)
    self.assertIsInstance(self.accumulated[0], RuntimeError)
    self.assertEquals(self.accumulated[0].message, 0)

  def testMultipleRejectResolve(self):
    (p, resolve, reject) = _GetPromiseAndFunctions()
    p.Then(self._AddToAccumulated, self._AddToAccumulated)
    resolve(0)
    self.assertEquals(self.accumulated, [0])
    resolve(0)
    self.assertEquals(self.accumulated, [0])
    reject(0)
    self.assertEquals(self.accumulated, [0])

    self.accumulated = []
    (p, resolve, reject) = _GetPromiseAndFunctions()
    p.Then(self._AddToAccumulated, self._AddToAccumulated)
    reject(0)
    self.assertEquals(self.accumulated, [0])
    resolve(0)
    self.assertEquals(self.accumulated, [0])
    reject(0)
    self.assertEquals(self.accumulated, [0])

  def testAll(self):
    promises_and_functions = [_GetPromiseAndFunctions() for x in xrange(10)]
    promises = [x[0] for x in promises_and_functions]
    all_promise = promise.Promise.All(*promises)
    res = []
    def AddToRes(values):
      res.append(values)
    all_promise.Then(AddToRes, AddToRes)
    for i, (_, resolve, _) in enumerate(promises_and_functions):
      self.assertEquals(len(res), 0)
      resolve(i)
    self.assertEquals(len(res), 1)
    self.assertEquals(res[0], [i for i in xrange(10)])
    self.assertEquals(all_promise.state, promise.Promise.STATE_FULLFILLED)

  def testAllFailure(self):
    promises_and_functions = [_GetPromiseAndFunctions() for x in xrange(10)]
    promises = [x[0] for x in promises_and_functions]
    all_promise = promise.Promise.All(*promises)
    res = []
    def AddToRes(values):
      res.append(values)
    all_promise.Then(AddToRes, AddToRes)
    for i in xrange(10):
      if i <= 5:
        self.assertEquals(len(res), 0)
      else:
        self.assertEquals(len(res), 1)
      if i != 5:
        promises_and_functions[i][1](i)
      else:
        promises_and_functions[i][2]('error')
    self.assertEquals(len(res), 1)
    self.assertEquals(res[0], 'error')
    self.assertEquals(all_promise.state, promise.Promise.STATE_REJECTED)

  def testRace(self):
    promises_and_functions = [_GetPromiseAndFunctions() for x in xrange(10)]
    promises = [x[0] for x in promises_and_functions]
    race_promise = promise.Promise.Race(*promises)
    res = []
    def AddToRes(values):
      res.append(values)
    race_promise.Then(AddToRes, AddToRes)
    self.assertEquals(len(res), 0)
    promises_and_functions[7][1]('success')
    self.assertEquals(len(res), 1)
    for i, (f) in enumerate(promises_and_functions):
      f[1 + (i % 2)](i)
    self.assertEquals(len(res), 1)
    self.assertEquals(res[0], 'success')
    self.assertEquals(race_promise.state, promise.Promise.STATE_FULLFILLED)

  def testRaceFailure(self):
    promises_and_functions = [_GetPromiseAndFunctions() for x in xrange(10)]
    promises = [x[0] for x in promises_and_functions]
    race_promise = promise.Promise.Race(*promises)
    res = []
    def AddToRes(values):
      res.append(values)
    race_promise.Then(AddToRes, AddToRes)
    self.assertEquals(len(res), 0)
    promises_and_functions[7][2]('error')
    self.assertEquals(len(res), 1)
    for i, (f) in enumerate(promises_and_functions):
      f[1 + (i % 2)](i)
    self.assertEquals(len(res), 1)
    self.assertEquals(res[0], 'error')
    self.assertEquals(race_promise.state, promise.Promise.STATE_REJECTED)


def _GetPromiseAndFunctions():
  functions = {}
  def GeneratorFunction(resolve, reject):
    functions['resolve'] = resolve
    functions['reject'] = reject
  p = promise.Promise(GeneratorFunction)
  return (p, functions['resolve'], functions['reject'])


def _ThrowException(x):
  raise RuntimeError(x)
