# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from __future__ import division
from __future__ import absolute_import

import logging

from dashboard.pinpoint.models import change as change_module
from dashboard.pinpoint.models import evaluators
from dashboard.pinpoint.models import event as event_module
from dashboard.pinpoint.models import job as job_module
from dashboard.pinpoint.models import task as task_module
from dashboard.pinpoint.models.tasks import bisection_test_util


class EvaluatorTest(bisection_test_util.BisectionTestBase):

  def setUp(self):
    super(EvaluatorTest, self).setUp()
    self.maxDiff = None
    self.job = job_module.Job.New((), ())

  def testPopulateWorks(self):
    self.PopulateSimpleBisectionGraph(self.job)

  def testEvaluateSuccess_NoReproduction(self):
    self.PopulateSimpleBisectionGraph(self.job)
    task_module.Evaluate(
        self.job,
        event_module.Event(type='initiate', target_task=None, payload={}),
        self.BisectionEvaluatorForTesting(
            bisection_test_util.FakeReadValueSameResult(self.job, 1.0)))
    evaluate_result = task_module.Evaluate(
        self.job,
        event_module.Event(type='select', target_task=None, payload={}),
        evaluators.Selector(task_type='find_culprit'))
    self.assertIn('performance_bisection', evaluate_result)
    logging.info('Results: %s', evaluate_result['performance_bisection'])
    self.assertEquals(evaluate_result['performance_bisection']['culprits'], [])

  def testEvaluateSuccess_SpeculateBisection(self):
    self.PopulateSimpleBisectionGraph(self.job)
    task_module.Evaluate(
        self.job,
        event_module.Event(type='initiate', target_task=None, payload={}),
        self.BisectionEvaluatorForTesting(
            bisection_test_util.FakeReadValueMapResult(
                self.job, {
                    change_module.Change.FromDict({
                        'commits': [{
                            'repository': 'chromium',
                            'git_hash': commit
                        }]
                    }): values for commit, values in (
                        ('commit_0', [1.0] * 10),
                        ('commit_1', [1.0] * 10),
                        ('commit_2', [2.0] * 10),
                        ('commit_3', [2.0] * 10),
                        ('commit_4', [2.0] * 10),
                        ('commit_5', [2.0] * 10),
                    )
                })))
    evaluate_result = task_module.Evaluate(
        self.job, bisection_test_util.SelectEvent(),
        evaluators.Selector(task_type='find_culprit'))
    self.assertIn('performance_bisection', evaluate_result)
    logging.info('Results: %s', evaluate_result['performance_bisection'])

    # Here we're testing that we can find the change between commit_1 and
    # commit_2 in the values we seed above.
    self.assertEquals(evaluate_result['performance_bisection']['culprits'], [[
        change_module.Change.FromDict({
            'commits': [{
                'repository': 'chromium',
                'git_hash': 'commit_1'
            }]
        }).AsDict(),
        change_module.Change.FromDict({
            'commits': [{
                'repository': 'chromium',
                'git_hash': 'commit_2'
            }]
        }).AsDict()
    ]])

  def testEvaluateSuccess_NeedToRefineAttempts(self):
    self.PopulateSimpleBisectionGraph(self.job)
    task_module.Evaluate(
        self.job,
        event_module.Event(type='initiate', target_task=None, payload={}),
        self.BisectionEvaluatorForTesting(
            bisection_test_util.FakeReadValueMapResult(
                self.job, {
                    change_module.Change.FromDict({
                        'commits': [{
                            'repository': 'chromium',
                            'git_hash': commit
                        }]
                    }): values for commit, values in (
                        ('commit_0', range(10)),
                        ('commit_1', range(10)),
                        ('commit_2', range(4, 14)),
                        ('commit_3', range(3, 13)),
                        ('commit_4', range(3, 13)),
                        ('commit_5', range(3, 13)),
                    )
                })))

    # Here we test that we have more than the minimum attempts for the change
    # between commit_1 and commit_2.
    evaluate_result = task_module.Evaluate(
        self.job, bisection_test_util.SelectEvent(),
        evaluators.Selector(task_type='read_value'))
    attempt_counts = {}
    for payload in evaluate_result.values():
      change = change_module.Change.FromDict(payload.get('change'))
      attempt_counts[change] = attempt_counts.get(change, 0) + 1
    self.assertGreater(
        attempt_counts[change_module.Change.FromDict(
            {'commits': [{
                'repository': 'chromium',
                'git_hash': 'commit_2',
            }]})], 10)
    self.assertLess(
        attempt_counts[change_module.Change.FromDict(
            {'commits': [{
                'repository': 'chromium',
                'git_hash': 'commit_2',
            }]})], 100)

    # We know that we will never get a deterministic answer, so we ensure that
    # we don't inadvertently blame the wrong changes at the end of the
    # refinement.
    evaluate_result = task_module.Evaluate(
        self.job, bisection_test_util.SelectEvent(),
        evaluators.Selector(task_type='find_culprit'))
    self.assertIn('performance_bisection', evaluate_result)
    logging.info('Results: %s', evaluate_result['performance_bisection'])
    self.assertEquals(evaluate_result['performance_bisection']['culprits'], [])

  def testEvaluateFailure_DependenciesFailed(self):
    self.PopulateSimpleBisectionGraph(self.job)
    task_module.Evaluate(
        self.job,
        event_module.Event(type='initiate', target_task=None, payload={}),
        self.BisectionEvaluatorForTesting(
            bisection_test_util.FakeReadValueFails(self.job)))
    evaluate_result = task_module.Evaluate(
        self.job, bisection_test_util.SelectEvent(),
        evaluators.Selector(task_type='find_culprit'))
    self.assertIn('performance_bisection', evaluate_result)
    self.assertEqual(evaluate_result['performance_bisection']['status'],
                     'failed')
    self.assertNotEqual([], evaluate_result['performance_bisection']['errors'])

  def testEvaluateFailure_DependenciesNoResults(self):
    self.PopulateSimpleBisectionGraph(self.job)
    task_module.Evaluate(
        self.job,
        event_module.Event(type='initiate', target_task=None, payload={}),
        self.BisectionEvaluatorForTesting(
            bisection_test_util.FakeReadValueSameResult(self.job, None)))
    evaluate_result = task_module.Evaluate(
        self.job, bisection_test_util.SelectEvent(),
        evaluators.Selector(task_type='find_culprit'))
    self.assertIn('performance_bisection', evaluate_result)
    self.assertEqual(evaluate_result['performance_bisection']['status'],
                     'failed')
    self.assertNotEqual([], evaluate_result['performance_bisection']['errors'])

  def testEvaluateAmbiguous_IntermediatePartialFailure(self):
    self.skipTest(
        'Implement the case where intermediary builds/tests failed but we can '
        'find some non-failing intermediary CLs')

  def testEvaluateAmbiguous_IntermediateCulpritIsAutoRoll(self):
    self.skipTest(
        'Implement the case where the likely culprit is an auto-roll commit, '
        'in which case we want to embellish the commit range with commits '
        'from the remote repositories')

  def testEvaluateAmbiguous_IntermediateCulpritFound_CancelOngoing(self):
    self.skipTest(
        'Implement the case where we have already found a culprit and we still '
        'have ongoing builds/tests running but have the chance to cancel '
        'those.')

  def testEvaluateFailure_ExtentClsFailed(self):
    self.skipTest(
        'Implement the case where either the start or end commits are broken.')
