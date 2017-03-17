# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Invokes git bisect to find culprit commit."""

from __future__ import print_function

import datetime

from chromite.cros_bisect import common
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git


class GitBisector(common.OptionsChecker):
  """Bisects suspicious commits in a git repository.

  Before bisect, it does sanity check for the given good and bad commit. Then
  it extracts metric scores for both commits as baseline to determine the
  upcoming bisect candidate's status.

  It finds first bad commit within given good and bad commit using "git bisect"
  command. For each commit, it uses builder to build package to verify and
  deploy it to DUT (device under test). And asks evaluator to extract score
  of the commit. It is treated as good commit i the score is closer to user
  specified good commit.

  Finally, it outputs git bisect result.
  """

  REQUIRED_ARGS = ('good', 'bad', 'remote', 'eval_repeat')

  def __init__(self, options, builder, evaluator):
    """Constructor.

    Args:
      options: An argparse.Namespace to hold command line arguments. Should
        contain:
        * good: Last known good commit.
        * bad: Last known bad commit.
        * remote: DUT (refer lib.commandline.Device).
        * eval_repeat: Run test for N times. None means 1.
      builder: Builder to build/deploy image. Should contain repo_dir.
      evaluator: Evaluator to get score
    """
    super(GitBisector, self).__init__(options)
    self.good_commit = options.good
    self.bad_commit = options.bad
    self.remote = options.remote
    self.eval_repeat = options.eval_repeat
    self.builder = builder
    self.evaluator = evaluator
    self.repo_dir = self.builder.repo_dir

    # Initialized in SanityCheck().
    self.good_score = None
    self.bad_score = None

    # If the distance between current commit's score and given good commit is
    # shorter than threshold, treat the commit as good one. The threshold is set
    # by user.
    self.threshold = None

    # Update in UpdateCurrentCommit().
    self.current_commit = common.CommitInfo()

  def SetUp(self):
    """Sets up environment to bisect."""
    logging.info('Set up builder environment.')
    self.builder.SetUp()

  def Git(self, command):
    """Wrapper of git.RunGit.

    It passes self.repo_dir as git_repo to git.RunGit.

    Args:
      command: git command.

    Returns:
      A CommandResult object.
    """
    return git.RunGit(self.repo_dir, command)

  def UpdateCurrentCommit(self):
    """Updates self.current_commit."""
    result = self.Git(['show', '--no-patch', '--format=%h %ct %s'])
    if result.returncode != 0:
      logging.error('Failed to update current commit info.')
      return

    fields = result.output.strip().split()
    if len(fields) < 3:
      logging.error('Failed to update current commit info.')
      return
    sha1 = fields[0]
    timestamp = int(fields[1])
    title = ' '.join(fields[2:])
    self.current_commit = common.CommitInfo(sha1=sha1, title=title,
                                            timestamp=timestamp)

  def GetCommitTimestamp(self, sha1):
    """Obtains given commit's timestamp.

    Args:
      sha1: Commit's SHA1 to look up.

    Returns:
      timestamp in integer. None if the commit does not exist.
    """
    result = self.Git(['show', '--no-patch', '--format=%ct', sha1])
    if result.returncode == 0:
      try:
        return int(result.output.strip())
      except ValueError:
        pass
    return None

  def SanityCheck(self):
    """Evaluates last known good and bad commit again.

    To make sure that good commit has better evaluation score on the DUT, it
    builds, deploys and evaluates both good and bad images on the DUT.

    Returns:
      (good_commit_info, bad_commit_info)
    """
    logging.info('Perform sanity check on good commit %s and bad commit %s',
                 self.good_commit, self.bad_commit)
    # Builder is lazy syncing to HEAD. Sync to HEAD if good_commit or bad_commit
    # does not exist in repo.
    if (not git.DoesCommitExistInRepo(self.repo_dir, self.good_commit) or
        not git.DoesCommitExistInRepo(self.repo_dir, self.bad_commit)):
      logging.info(
          'Either good commit (%s) or bad commit (%s) not found in repo. '
          'Try syncing to HEAD first.', self.good_commit, self.bad_commit)
      self.builder.SyncToHead()
    else:
      logging.info(
          'Both good commit (%s) and bad commit (%s) are found in repo. '
          'No need to update repo.', self.good_commit, self.bad_commit)

    good_commit_timestamp = self.GetCommitTimestamp(self.good_commit)
    if good_commit_timestamp is None:
      logging.error('Sanity check failed: good commit %s does not exist.',
                    self.good_commit)
      return None, None
    bad_commit_timestamp = self.GetCommitTimestamp(self.bad_commit)
    if bad_commit_timestamp is None:
      logging.error('Sanity check failed: bad commit %s does not exist.',
                    self.bad_commit)
      return None, None

    # TODO(deanliao): Consider the case that we want to find a commit that
    #     fixed a problem.
    if bad_commit_timestamp < good_commit_timestamp:
      logging.error(
          'Sanity check failed: good commit (%s) timestamp: %s should be '
          'earlier than bad commit (%s): %s',
          self.good_commit, good_commit_timestamp,
          self.bad_commit, bad_commit_timestamp)
      return None, None

    logging.notice('Obtaining score of good commit %s', self.good_commit)
    self.Git(['checkout', self.good_commit])
    self.good_score = self.BuildDeployEval()
    good_commit_info = self.current_commit
    good_commit_info.label = 'last-known-good  '

    logging.notice('Obtaining score of bad commit %s', self.bad_commit)
    self.Git(['checkout', self.bad_commit])
    self.bad_score = self.BuildDeployEval()
    bad_commit_info = self.current_commit
    bad_commit_info.label = 'last-known-bad   '
    return (good_commit_info, bad_commit_info)

  def GetThresholdFromUser(self):
    """Gets threshold that decides good/bad commit.

    It gives user the measured last known good and bad score's statistics to
    help user determine a reasonable threshold.
    """
    logging.notice('Good commit score mean: %.3f  STD: %.3f\n'
                   'Bad commit score mean: %.3f  STD: %.3f',
                   self.good_score.mean, self.good_score.std,
                   self.bad_score.mean, self.bad_score.std)

    ref_score_min = min(self.good_score.mean, self.bad_score.mean)
    ref_score_max = max(self.good_score.mean, self.bad_score.mean)
    success = False
    retry = 3
    for _ in range(retry):
      try:
        splitter = float(cros_build_lib.GetInput(
            'Please give a threshold that tell apart good and bad commit '
            '(within range [%.3f, %.3f]: ' % (ref_score_min, ref_score_max)))
      except ValueError:
        logging.error('Threshold should be a floating number.')
        continue

      if ref_score_min <= splitter <= ref_score_max:
        self.threshold = abs(self.good_score.mean - splitter)
        success = True
        break
      else:
        logging.error('Threshold should be within range [%.3f, %.3f]',
                      ref_score_min, ref_score_max)
    else:
      logging.error('Wrong threshold input for %d times. Terminate.', retry)

    return success

  def BuildDeployEval(self, raise_on_error=True):
    """Builds the image, deploys to DUT and evaluates performance.

    Args:
      raise_on_error: If set, raises Exception if score is unable to get.

    Returns:
      Evaluation result.

    Raises:
      Exception if raise_on_error and score is unable to get.
    """
    self.UpdateCurrentCommit()
    last_score = self.evaluator.CheckLastEvaluate(self.current_commit.sha1,
                                                  self.eval_repeat)
    if len(last_score) > 0:
      self.current_commit.score = last_score
      return last_score

    # TODO(deanliao): handle build/deploy fail case.
    build_to_deploy = self.builder.Build(self.current_commit.sha1)
    self.builder.Deploy(self.remote, build_to_deploy, self.current_commit.sha1)
    score = self.evaluator.Evaluate(self.remote, self.current_commit.sha1,
                                    self.eval_repeat)
    self.current_commit.score = score
    if not score and raise_on_error:
      raise Exception('Unable to obtain evaluation score')
    return score

  def LabelBuild(self, score):
    """Determines if a build is good.

    Args:
      score: evaluation score of the build.

    Returns:
      'good' if the score is closer to given good one; otherwise (including
      score is None), 'bad'.
    """
    label = 'bad'
    if not score:
      logging.error('No score. Marked as bad.')
      return label

    ref_score_min = min(self.good_score.mean, self.bad_score.mean)
    ref_score_max = max(self.good_score.mean, self.bad_score.mean)
    if ref_score_min < score.mean < ref_score_max:
      # Within (good_score, bad_score)
      if abs(self.good_score.mean - score.mean) <= self.threshold:
        label = 'good'
    else:
      dist_good = abs(self.good_score.mean - score.mean)
      dist_bad = abs(self.bad_score.mean - score.mean)
      if dist_good < dist_bad:
        label = 'good'
    logging.info('Score %.3f marked as %s', score.mean, label)
    return label

  def GitBisect(self, bisect_op):
    """Calls git bisect and logs result.

    Args:
      bisect_op: git bisect subcommand list.

    Returns:
      A CommandResult object.
      done: True if bisect ends.
    """
    command = ['bisect'] + bisect_op
    command_str = cros_build_lib.CmdToStr(['git'] + command)
    result = self.Git(command)
    done = False
    log_msg = [command_str]
    if result.output:
      log_msg.append('(output): %s' % result.output.strip())
    if result.error:
      log_msg.append('(error): %s' % result.error.strip())
    if result.returncode != 0:
      log_msg.append('(returncode): %d' % result.returncode)

    log_msg = '\n'.join(log_msg)
    if result.error or result.returncode != 0:
      logging.error(log_msg)
    else:
      logging.info(log_msg)

    if result.output:
      if 'is the first bad commit' in result.output.splitlines()[0]:
        done = True
    return result, done

  def Run(self):
    """Bisects culprit commit.

    Returns:
      Culprit commit's SHA1. None if culprit not found.
    """
    def CommitInfoToStr(info):
      timestamp = datetime.datetime.fromtimestamp(info.timestamp).isoformat(' ')
      score = 'Score(mean: %.3f std: %.3f)' % (info.score.mean, info.score.std)
      return ' '.join([info.label, timestamp, score, info.sha1, info.title])

    (good_commit_info, bad_commit_info) = self.SanityCheck()
    if good_commit_info is None or bad_commit_info is None:
      # logging.error was already called in SanityCheck.
      return None
    if not self.GetThresholdFromUser():
      return None
    self.GitBisect(['reset'])
    bisect_done = False
    culprit_commit = None
    bisect_log = [good_commit_info, bad_commit_info]
    nth_bisect = 0
    try:
      with cros_build_lib.TimedSection() as timer:
        self.GitBisect(['start'])
        self.GitBisect(['bad', self.bad_commit])
        self.GitBisect(['good', self.good_commit])
        while not bisect_done:
          metric_score = self.BuildDeployEval()
          bisect_op = self.LabelBuild(metric_score)
          nth_bisect += 1
          self.current_commit.label = '%2d-th-bisect-%-4s' % (nth_bisect,
                                                              bisect_op)
          bisect_log.append(self.current_commit)
          logging.notice('Mark %s as %s. Score: %s', self.current_commit.sha1,
                         bisect_op, metric_score)
          bisect_result, bisect_done = self.GitBisect([bisect_op])
          if bisect_done:
            culprit_commit = bisect_result.output.splitlines()[1:]
      if bisect_done:
        logging.info('git bisect done. Elapsed time: %s', timer.delta)
    finally:
      self.GitBisect(['log'])
      self.GitBisect(['reset'])

    bisect_log.sort(key=lambda x: x.timestamp)
    logging.notice(
        'Bisect log (sort by time):\n' +
        '\n'.join(map(CommitInfoToStr, bisect_log)))

    if culprit_commit:
      logging.notice('\n'.join(['Culprit commit:'] + culprit_commit))
      # First line: commit <SHA>
      return culprit_commit[0].split()[1]
    else:
      logging.notice('Culprit not found.')
      return None
